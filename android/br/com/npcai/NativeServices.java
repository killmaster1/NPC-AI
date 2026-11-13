package br.com.npcai;

import android.content.Context;
import android.app.Application;
import android.app.Activity;
import android.os.Bundle;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.security.*;
import java.security.spec.AlgorithmParameterSpec;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicInteger;
import javax.crypto.*;

import javax.net.ssl.*;


/** Network/storage only. No Activity, no TLS overrides, no native engine callbacks. */
public final class NativeServices {
    private static Context app;private static boolean lifecycleInstalled=false;
    private static native void pauseMemory();
    private static volatile String plainKey="";
    private static volatile boolean protectedKey=true, internet=false, credentialBlocked=true;
    private static volatile int internetState=-1;
    private static volatile boolean credentialAvailable=false;
    private static final String ALIAS="npcai.gemini.v1";
    private static File root;
    private static final Semaphore slots=new Semaphore(2);
    private static final ConcurrentHashMap<Integer,Job> jobs=new ConcurrentHashMap<Integer,Job>();
    private static final AtomicInteger sequence=new AtomicInteger();
    private static final ThreadPoolExecutor closer=new ThreadPoolExecutor(1,1,0,TimeUnit.SECONDS,new ArrayBlockingQueue<Runnable>(2),new ThreadFactory(){public Thread newThread(Runnable r){Thread t=new Thread(r,"NPC-AI-cancel");t.setDaemon(true);return t;}},new ThreadPoolExecutor.DiscardPolicy());
    private NativeServices(){}
    private static byte[] utf(String s){return s.getBytes(StandardCharsets.UTF_8);}
    private static String str(byte[] b){return b==null?"":new String(b,StandardCharsets.UTF_8);}
    private static String quote(String s){StringBuilder out=new StringBuilder("\"");for(int i=0;i<s.length();i++){char c=s.charAt(i);if(c=='"' || c=='\\')out.append('\\').append(c);else if(c<32)out.append(String.format("\\u%04x",(int)c));else out.append(c);}return out.append('"').toString();}
    private static byte[] envelope(int status,String body,String error){return utf("{\"status\":"+status+",\"body\":"+quote(body)+",\"error\":"+quote(error)+"}");}

    static boolean validCredential(String value){return value!=null && value.matches("[A-Za-z0-9_.-]{1,256}");}
    public static synchronized byte[] status(){
        return utf("{\"https_available\":true,\"internet\":"+internetState+",\"credential_present\":"+credentialAvailable+"}");
    }
    public static synchronized byte[] configure(Context context,byte[] secret,String storage,String action){
        credentialBlocked=true;credentialAvailable=false;plainKey="";internet=false;internetState=-1;
        try{
            if(context==null)return envelope(0,"","jni_configuration");
            app=context.getApplicationContext();if(app==null)app=context;
            try{internet=app.checkCallingOrSelfPermission("android.permission.INTERNET")==0;internetState=internet?1:0;}catch(Throwable ignored){}
            if(!lifecycleInstalled && app instanceof Application){
                try{((Application)app).registerActivityLifecycleCallbacks(new Application.ActivityLifecycleCallbacks(){
                    public void onActivityCreated(Activity a,Bundle b){}public void onActivityStarted(Activity a){}public void onActivityResumed(Activity a){}
                    public void onActivityPaused(Activity a){try{pauseMemory();}catch(Throwable ignored){}}
                    public void onActivityStopped(Activity a){}public void onActivitySaveInstanceState(Activity a,Bundle b){}
                    public void onActivityDestroyed(Activity a){try{pauseMemory();}catch(Throwable ignored){}}
                });lifecycleInstalled=true;}catch(Throwable ignored){}
            }
            root=null;
            try{File directory=(File)Context.class.getMethod("getNoBackupFilesDir").invoke(app);if(directory!=null){root=new File(directory,"npcai");if(!root.isDirectory() && !root.mkdirs())root=null;}}catch(Throwable ignored){}
            final String directory=root==null?"":root.getAbsolutePath();
            protectedKey=!"ini".equals(storage);
            if("remove".equals(action)){eraseCredential();return envelope(200,directory,internet?"credential_removed":"permission");}
            String value=str(secret);
            if(!value.isEmpty() && !validCredential(value))return envelope(0,directory,"credential_format");
            String credentialState="";
            if(protectedKey){
                if(root==null)return envelope(0,"","private_storage");
                if("replace".equals(action)){if(value.isEmpty())return envelope(0,directory,"credential_missing");store(value);credentialState="credential_stored";}
            }else {plainKey=value;if(!value.isEmpty())credentialState="plaintext_credential_in_use";}
            credentialBlocked=false;
            try{credentialAvailable=!credential().isEmpty();}catch(Exception ignored){credentialAvailable=false;}
            if(!credentialAvailable)credentialState="credential_missing";
            return envelope(200,directory,internet?credentialState:"permission");
        }catch(Throwable ignored){credentialBlocked=true;credentialAvailable=false;return envelope(0,root==null?"":root.getAbsolutePath(),"jni_configuration");}
        finally{if(secret!=null)java.util.Arrays.fill(secret,(byte)0);}
    }
    private static SecretKey key(boolean create)throws Exception {
        KeyStore ks=KeyStore.getInstance("AndroidKeyStore");ks.load(null);
        if(ks.containsAlias(ALIAS))return (SecretKey)ks.getKey(ALIAS,null);
        if(!create)throw new GeneralSecurityException();
        Class<?> props=Class.forName("android.security.keystore.KeyProperties");
        int purpose=props.getField("PURPOSE_ENCRYPT").getInt(null)|props.getField("PURPOSE_DECRYPT").getInt(null);
        Class<?> builderClass=Class.forName("android.security.keystore.KeyGenParameterSpec$Builder");
        Object builder=builderClass.getConstructor(String.class,int.class).newInstance(ALIAS,purpose);
        builderClass.getMethod("setKeySize",int.class).invoke(builder,256);
        builderClass.getMethod("setBlockModes",String[].class).invoke(builder,(Object)new String[]{"GCM"});
        builderClass.getMethod("setEncryptionPaddings",String[].class).invoke(builder,(Object)new String[]{"NoPadding"});
        AlgorithmParameterSpec spec=(AlgorithmParameterSpec)builderClass.getMethod("build").invoke(builder);
        KeyGenerator generator=KeyGenerator.getInstance("AES","AndroidKeyStore");generator.init(spec);return generator.generateKey();
    }
    private static byte[] seal(SecretKey secret,String value)throws Exception {
        Cipher cipher=Cipher.getInstance("AES/GCM/NoPadding");cipher.init(Cipher.ENCRYPT_MODE,secret);
        byte[] iv=cipher.getIV(),encrypted=cipher.doFinal(utf(value));ByteArrayOutputStream bytes=new ByteArrayOutputStream();bytes.write(1);bytes.write(iv.length);bytes.write(iv);bytes.write(encrypted);return bytes.toByteArray();
    }
    private static String unseal(SecretKey secret,byte[] bytes)throws Exception {
        if(bytes.length<30 || bytes.length>1024 || bytes[0]!=1 || bytes[1]!=12)throw new GeneralSecurityException();
        byte[] iv=java.util.Arrays.copyOfRange(bytes,2,14);
        AlgorithmParameterSpec spec=(AlgorithmParameterSpec)Class.forName("javax.crypto.spec.GCMParameterSpec").getConstructor(int.class,byte[].class).newInstance(128,iv);
        Cipher cipher=Cipher.getInstance("AES/GCM/NoPadding");cipher.init(Cipher.DECRYPT_MODE,secret,spec);return str(cipher.doFinal(bytes,14,bytes.length-14));
    }
    private static void store(String secret)throws Exception {
        byte[] bytes=seal(key(true),secret);File tmp=new File(root,"credential.tmp"),file=new File(root,"credential.enc");
        FileOutputStream out=new FileOutputStream(tmp);try{out.write(bytes);out.getFD().sync();}finally{out.close();}
        if(!tmp.renameTo(file)){tmp.delete();throw new IOException();}
    }
    private static String credential()throws Exception {
        if(credentialBlocked)throw new GeneralSecurityException();
        if(!protectedKey)return plainKey;
        File file=new File(root,"credential.enc");if(!file.isFile() || file.length()>1024)throw new GeneralSecurityException();
        byte[] bytes=new byte[(int)file.length()];FileInputStream in=new FileInputStream(file);
        try{int n=0;while(n<bytes.length){int r=in.read(bytes,n,bytes.length-n);if(r<0)throw new IOException();n+=r;}}finally{in.close();}
        return unseal(key(false),bytes);
    }

    private static void eraseCredential()throws Exception {
        plainKey="";KeyStore ks=KeyStore.getInstance("AndroidKeyStore");ks.load(null);if(ks.containsAlias(ALIAS))ks.deleteEntry(ALIAS);
        if(root!=null){new File(root,"credential.enc").delete();new File(root,"credential.tmp").delete();}
    }
    private static int retryDelayMs(String value){
        if(value==null)return 0;
        try{if(value.matches("[0-9]{1,5}"))return Math.min(60000,Integer.parseInt(value)*1000);
            java.text.SimpleDateFormat format=new java.text.SimpleDateFormat("EEE, dd MMM yyyy HH:mm:ss zzz",java.util.Locale.US);format.setLenient(false);long delay=format.parse(value).getTime()-System.currentTimeMillis();return (int)Math.max(0,Math.min(60000,delay));
        }catch(Exception e){return 0;}
    }
    public static int start(String model,byte[] payload,int timeout){
        if(!internet || model==null || !model.matches("[A-Za-z0-9._-]{1,100}") || payload==null || payload.length>65536 || timeout<1 || timeout>60000 || !slots.tryAcquire())return -1;
        int id=sequence.incrementAndGet();Job job=new Job(id,model,payload,timeout);jobs.put(id,job);
        Thread thread=new Thread(job,"NPC-AI-HTTPS");thread.setDaemon(true);job.thread=thread;thread.start();return id;
    }
    public static byte[] poll(int id){Job job=jobs.get(id);if(job==null)return envelope(0,"","cancelled");byte[] result=job.result;if(result!=null)jobs.remove(id);return result;}
    public static void cancel(int id){final Job job=jobs.remove(id);if(job==null)return;job.cancelled=true;if(job.thread!=null)job.thread.interrupt();closer.execute(new Runnable(){public void run(){HttpsURLConnection c=job.connection;if(c!=null)c.disconnect();}});}
    private static final class Job implements Runnable {
        final int id,timeout;final String model;final byte[] payload;volatile boolean cancelled=false;volatile byte[] result;volatile HttpsURLConnection connection;volatile Thread thread;
        Job(int id,String model,byte[] payload,int timeout){this.id=id;this.model=model;this.payload=payload;this.timeout=timeout;}
        public void run(){
            long deadline=System.nanoTime()+timeout*1000000L;
            try{
                String secret=credential();if(secret.isEmpty()){result=envelope(0,"","credential_missing");return;}
                URL url=new URL("https://generativelanguage.googleapis.com/v1beta/models/"+model+":generateContent");
                connection=(HttpsURLConnection)url.openConnection();
                connection.setInstanceFollowRedirects(false);connection.setRequestMethod("POST");connection.setDoOutput(true);
                connection.setConnectTimeout(Math.min(8000,timeout));connection.setReadTimeout(timeout);
                connection.setRequestProperty("Content-Type","application/json; charset=utf-8");connection.setRequestProperty("x-goog-api-key",secret);
                connection.setFixedLengthStreamingMode(payload.length);
                if(cancelled)return;
                OutputStream output=connection.getOutputStream();try{output.write(payload);}finally{output.close();}
                int remaining=(int)Math.max(1,(deadline-System.nanoTime())/1000000);connection.setReadTimeout(remaining);int status=connection.getResponseCode();
                InputStream input=status>=200 && status<300?connection.getInputStream():connection.getErrorStream();
                ByteArrayOutputStream body=new ByteArrayOutputStream();byte[] buffer=new byte[4096];
                if(input!=null)try{while(!cancelled){long ms=(deadline-System.nanoTime())/1000000;if(ms<=0)throw new SocketTimeoutException();connection.setReadTimeout((int)Math.min(ms,60000));int n=input.read(buffer);if(n<0)break;if(body.size()+n>262144)throw new IOException();body.write(buffer,0,n);}}finally{input.close();}
                if(!cancelled){String wire=str(envelope(status,str(body.toByteArray()),""));String retry=connection.getHeaderField("Retry-After");if(retry!=null)wire=wire.substring(0,wire.length()-1)+",\"retry_after_ms\":"+retryDelayMs(retry)+"}";result=utf(wire);}
            }catch(SSLException e){result=envelope(0,"","tls");}
            catch(SocketTimeoutException e){result=envelope(0,"","timeout");}
            catch(GeneralSecurityException e){result=envelope(0,"","credential_missing");}
            catch(SecurityException e){result=envelope(0,"","permission");}
            catch(Exception e){result=envelope(0,"","network");}
            finally{HttpsURLConnection c=connection;if(c!=null)c.disconnect();slots.release();if(cancelled)jobs.remove(id);}
        }
    }
}
