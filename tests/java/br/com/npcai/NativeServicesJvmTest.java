package br.com.npcai;
import java.lang.reflect.*;
import java.net.*;
import java.io.*;
import java.security.cert.Certificate;
import javax.net.ssl.*;
import javax.crypto.*;
import javax.crypto.spec.*;
public final class NativeServicesJvmTest {
    static String mode="ok";static boolean disconnected=false;
    static void field(String name,Object value)throws Exception{Field f=NativeServices.class.getDeclaredField(name);f.setAccessible(true);f.set(null,value);}
    static void check(boolean b){if(!b)throw new AssertionError();}
    static final class Fake extends HttpsURLConnection {
        Fake(URL u){super(u);}
        public void connect(){}public void disconnect(){disconnected=true;}public boolean usingProxy(){return false;}
        public String getCipherSuite(){return "test";}public Certificate[] getLocalCertificates(){return null;}public Certificate[] getServerCertificates(){return null;}
        public OutputStream getOutputStream()throws IOException{check(getSSLSocketFactory()==HttpsURLConnection.getDefaultSSLSocketFactory());check(getHostnameVerifier()==HttpsURLConnection.getDefaultHostnameVerifier());check(!getInstanceFollowRedirects());return new ByteArrayOutputStream();}
        public int getResponseCode()throws IOException{if(mode.equals("block"))try{Thread.sleep(1000);}catch(InterruptedException e){throw new IOException();}if(mode.equals("tls"))throw new SSLHandshakeException("sensitive-key-never-log");if(mode.equals("timeout"))throw new SocketTimeoutException("sensitive-key-never-log");if(mode.equals("network"))throw new IOException("sensitive-key-never-log");return mode.equals("429")?429:200;}
        public String getHeaderField(String name){return "Retry-After".equals(name)?"2":null;}
        public InputStream getErrorStream(){return new ByteArrayInputStream("error".getBytes());}
        public InputStream getInputStream(){return new ByteArrayInputStream("Olá! ação ç".getBytes(java.nio.charset.StandardCharsets.UTF_8));}
    }
    static String reply(String next)throws Exception{mode=next;int id=NativeServices.start("gemini-test","{}".getBytes(),1000);check(id>0);for(int i=0;i<200;++i){byte[] b=NativeServices.poll(id);if(b!=null)return new String(b,java.nio.charset.StandardCharsets.UTF_8);Thread.sleep(5);}throw new AssertionError("timeout");}
    public static void main(String[] args)throws Exception {
        check(NativeServices.validCredential("fake.test-key_123"));
        for(String invalid:new String[]{"fake key","fake\r\nkey","fake/key","\"fake.test-key\""})check(!NativeServices.validCredential(invalid));
        check(!NativeServices.validCredential(new String(new char[257]).replace('\0','A')));
        byte[] invalidSecret="never-print-this-private-key".getBytes();
        String failedConfig=new String(NativeServices.configure(null,invalidSecret,"ini","none"),java.nio.charset.StandardCharsets.UTF_8);
        check(failedConfig.contains("jni_configuration") && !failedConfig.contains("never-print-this-private-key"));
        for(byte b:invalidSecret)check(b==0);
        String status=new String(NativeServices.status(),java.nio.charset.StandardCharsets.UTF_8);
        check(status.contains("credential_present\":false") && status.contains("internet\":-1") && !status.contains("plainKey"));
        URL.setURLStreamHandlerFactory(new URLStreamHandlerFactory(){public URLStreamHandler createURLStreamHandler(String protocol){if(!protocol.equals("https"))return null;return new URLStreamHandler(){protected URLConnection openConnection(URL u){check(u.getHost().equals("generativelanguage.googleapis.com"));return new Fake(u);}};}});
        field("internet",false);check(NativeServices.start("gemini-test","{}".getBytes(),1000)<0);
        field("internet",true);field("credentialBlocked",false);field("protectedKey",false);field("plainKey","sensitive-key-never-log");
        check(NativeServices.start("../invalid","{}".getBytes(),1000)<0);check(NativeServices.start("gemini-test",new byte[65537],1000)<0);
        check(reply("ok").contains("Olá! ação ç"));
        for(String m:new String[]{"tls","timeout","network"}){String result=reply(m);check(result.contains(m));check(!result.contains("sensitive-key-never-log"));}
        check(reply("429").contains("2000"));
        field("plainKey","");check(reply("ok").contains("credential_missing"));
        field("plainKey","new-key");check(reply("ok").contains("status\":200"));
        mode="block";int cancelled=NativeServices.start("gemini-test","{}".getBytes(),2000);check(cancelled>0);long before=System.nanoTime();NativeServices.cancel(cancelled);check((System.nanoTime()-before)/1000000<250);check(new String(NativeServices.poll(cancelled),java.nio.charset.StandardCharsets.UTF_8).contains("cancelled"));field("plainKey","");
        Method retry=NativeServices.class.getDeclaredMethod("retryDelayMs",String.class);retry.setAccessible(true);check(((Integer)retry.invoke(null,"2"))==2000);check(((Integer)retry.invoke(null,"invalid"))==0);

        // Validate the standard AES-GCM algorithm independently; AndroidKeyStore provider needs a device.
        byte[] key=new byte[32];new java.security.SecureRandom().nextBytes(key);SecretKeySpec secret=new SecretKeySpec(key,"AES");
        Method seal=NativeServices.class.getDeclaredMethod("seal",SecretKey.class,String.class),open=NativeServices.class.getDeclaredMethod("unseal",SecretKey.class,byte[].class);seal.setAccessible(true);open.setAccessible(true);
        byte[] encrypted=(byte[])seal.invoke(null,secret,"fake-key");check(open.invoke(null,secret,encrypted).equals("fake-key"));encrypted[20]^=1;boolean rejected=false;try{open.invoke(null,secret,encrypted);}catch(InvocationTargetException e){rejected=e.getCause() instanceof AEADBadTagException;}check(rejected);
        field("credentialBlocked",true);field("plainKey","old-credential");check(reply("ok").contains("credential_missing"));

        System.out.println("JVM HTTPS policy, permission gate, UTF-8, credential replacement/removal in memory, errors, limits and AES-GCM tamper checks passed; AndroidKeyStore/JNI/device not validated");
    }
}
