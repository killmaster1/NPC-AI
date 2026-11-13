package br.com.npcai;
import java.lang.reflect.*;import java.net.*;import java.io.*;import java.security.cert.Certificate;import javax.net.ssl.*;import javax.crypto.*;import javax.crypto.spec.*;
public final class NativeElevenJvmTest {
 static String mode="ok";static int attempts=0;static boolean disconnected=false;
 static void check(boolean x){if(!x)throw new AssertionError();}
 static void field(String n,Object v)throws Exception{Field f=NativeEleven.class.getDeclaredField(n);f.setAccessible(true);f.set(null,v);}
 static class Fake extends HttpsURLConnection {
  Fake(URL u){super(u);check(u.getHost().equals("api.elevenlabs.io"));check(!u.toString().contains("fixture-key"));}
  public void connect(){}public void disconnect(){disconnected=true;}public boolean usingProxy(){return false;}public String getCipherSuite(){return "fixture";}public Certificate[] getLocalCertificates(){return null;}public Certificate[] getServerCertificates(){return null;}
  public OutputStream getOutputStream(){check(getSSLSocketFactory()==HttpsURLConnection.getDefaultSSLSocketFactory());check(getHostnameVerifier()==HttpsURLConnection.getDefaultHostnameVerifier());check(!getInstanceFollowRedirects());check("fixture-key".equals(getRequestProperty("xi-api-key")));check(url.getQuery().contains("pcm_24000"));return new ByteArrayOutputStream();}
  public int getResponseCode()throws IOException{++attempts;if(mode.equals("tls"))throw new SSLHandshakeException("fixture-key-never-echo");if(mode.equals("timeout"))throw new SocketTimeoutException("fixture-key-never-echo");if(mode.equals("block"))try{Thread.sleep(1000);}catch(Exception e){}try{return Integer.parseInt(mode);}catch(Exception e){return 200;}}
  public String getContentType(){return mode.equals("mp3")?"audio/mpeg":"audio/pcm";}
  public InputStream getInputStream(){return new ByteArrayInputStream(new byte[mode.equals("odd")?3:4800]);}
 }
 static String reply(String next)throws Exception{mode=next;int id=NativeEleven.start("FixtureVoice","{\"text\":\"ação\"}".getBytes("UTF-8"),2000);check(id>0);for(int i=0;i<400;++i){byte[] b=NativeEleven.poll(id);if(b!=null){String s=new String(b,"UTF-8");byte[] pcm=NativeEleven.audio(id);if(next.equals("ok") && s.contains("\"status\":200"))check(pcm!=null && pcm.length==4800);return s;}Thread.sleep(5);}throw new AssertionError("deadline");}
 public static void main(String[] args)throws Exception{
  check(NativeEleven.validCredential("fixture-key"));check(!NativeEleven.validCredential("key\r\nheader"));byte[] invalid="fixture-key".getBytes("UTF-8");NativeEleven.configure(null,invalid,"ini","none");for(byte b:invalid)check(b==0);
  URL.setURLStreamHandlerFactory(new URLStreamHandlerFactory(){public URLStreamHandler createURLStreamHandler(String p){return p.equals("https")?new URLStreamHandler(){protected URLConnection openConnection(URL u){return new Fake(u);}}:null;}});
  field("internet",true);field("credentialBlocked",false);field("protectedKey",false);field("plainKey","fixture-key");
  check(NativeEleven.start("../escape",new byte[0],1000)<0);check(NativeEleven.start("FixtureVoice",new byte[16385],1000)<0);
  check(reply("ok").contains("200"));for(String m:new String[]{"401","402","403","429","tls","timeout","mp3","odd"}){int old=attempts;String s=reply(m);check(attempts==old+1 && !s.contains("fixture-key"));check(s.contains(m.equals("mp3") || m.equals("odd")?"audio_format":m));}
  field("plainKey","");check(reply("ok").contains("credential_missing"));field("plainKey","fixture-key");
  SecretKeySpec key=new SecretKeySpec(new byte[32],"AES");Method seal=NativeEleven.class.getDeclaredMethod("seal",SecretKey.class,String.class),open=NativeEleven.class.getDeclaredMethod("unseal",SecretKey.class,byte[].class);seal.setAccessible(true);open.setAccessible(true);byte[] encrypted=(byte[])seal.invoke(null,key,"fixture-key");check(open.invoke(null,key,encrypted).equals("fixture-key"));encrypted[encrypted.length-1]^=1;boolean rejected=false;try{open.invoke(null,key,encrypted);}catch(InvocationTargetException e){rejected=true;}check(rejected);
  mode="block";int id=NativeEleven.start("FixtureVoice",new byte[0],2000);check(id>0);long before=System.nanoTime();NativeEleven.cancel(id);check((System.nanoTime()-before)/1000000<250);check(new String(NativeEleven.poll(id),"UTF-8").contains("cancelled"));
  System.out.println("Eleven HTTPS JVM: TLS defaults, PCM format, header isolation, no retries, errors, cancellation and AES-GCM passed with mocks; Android Keystore/device pending");
 }
}
