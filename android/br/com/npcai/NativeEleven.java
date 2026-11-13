package br.com.npcai;
import android.content.Context;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.security.*;
import java.security.spec.AlgorithmParameterSpec;
import javax.crypto.*;
import javax.net.ssl.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicInteger;

/** Independent ElevenLabs credential, HTTP queue and cancellation. Never creates an Activity. */
public final class NativeEleven {
 private static final String ALIAS="npcai.elevenlabs.v1";
 private static boolean observed=false;private static File root;private static volatile String plainKey="";private static boolean protectedKey=true,credentialBlocked=true;private static volatile boolean internet=false;
 private static final Semaphore slots=new Semaphore(1);private static final ConcurrentHashMap<Integer,Job> jobs=new ConcurrentHashMap<Integer,Job>();private static final AtomicInteger sequence=new AtomicInteger();
 private static byte[] utf(String s){return s.getBytes(StandardCharsets.UTF_8);}private static String str(byte[] b){return b==null?"":new String(b,StandardCharsets.UTF_8);}
 static boolean validCredential(String s){return s!=null && s.matches("[A-Za-z0-9_.-]{1,512}");}
 private static String quote(String value){StringBuilder s=new StringBuilder("\"");for(int i=0;i<value.length();i++){char c=value.charAt(i);if(c=='"' || c=='\\')s.append('\\').append(c);else if(c<32)s.append(String.format("\\u%04x",(int)c));else s.append(c);}return s.append('"').toString();}
 private static byte[] info(int code,String error,String body){return utf("{\"status\":"+code+",\"error\":"+quote(error)+",\"body\":"+quote(body)+"}");}
 public static byte[] configure(Context c,byte[] secret,String storage,String action){try{if(c==null)return info(0,"jni","");Context app=c.getApplicationContext();if(app==null)app=c;internet=app.checkCallingOrSelfPermission("android.permission.INTERNET")==0;
  if(!observed && app instanceof android.app.Application){((android.app.Application)app).registerActivityLifecycleCallbacks(new android.app.Application.ActivityLifecycleCallbacks(){public void onActivityCreated(android.app.Activity a,android.os.Bundle b){}public void onActivityStarted(android.app.Activity a){}public void onActivityResumed(android.app.Activity a){}public void onActivityPaused(android.app.Activity a){for(Integer id:jobs.keySet())cancel(id);NativeAudio.close();}public void onActivityStopped(android.app.Activity a){}public void onActivitySaveInstanceState(android.app.Activity a,android.os.Bundle b){}public void onActivityDestroyed(android.app.Activity a){for(Integer id:jobs.keySet())cancel(id);NativeAudio.close();}});observed=true;}
  File parent=(File)Context.class.getMethod("getNoBackupFilesDir").invoke(app);root=new File(parent,"npcai-elevenlabs");if(!root.isDirectory() && !root.mkdirs())return info(0,"private_storage","");protectedKey=!"ini".equals(storage);plainKey="";credentialBlocked=false;
  if(new File(root,"credential.removed").exists() && !"replace".equals(action) && !"remove".equals(action)){credentialBlocked=true;return control("status",new byte[0]);}
  String key=str(secret);if(!key.isEmpty() && !validCredential(key)){credentialBlocked=true;return info(0,"credential_format","");}
  if("replace".equals(action) && !key.isEmpty())new File(root,"credential.removed").delete();
  if("remove".equals(action))eraseCredential();else if(protectedKey && "replace".equals(action)){if(key.isEmpty())return info(0,"credential_missing","");store(key);}else if(!protectedKey)plainKey=key;
  return control("status",new byte[0]);
 }catch(Throwable ignored){credentialBlocked=true;return info(0,"configuration","");}finally{if(secret!=null)java.util.Arrays.fill(secret,(byte)0);}}
 public static synchronized byte[] control(String op,byte[] value){String error="";try{
  if("import_key".equals(op)){String key=str(value);if(!validCredential(key))throw new GeneralSecurityException();if(root==null)throw new IOException();store(key);new File(root,"credential.removed").delete();protectedKey=true;plainKey="";credentialBlocked=false;}
  else if("remove_key".equals(op))eraseCredential();
  else if(!"status".equals(op))return info(0,"operation","");
  boolean present=false;try{present=!credential().isEmpty();}catch(Exception ignored){}
  return utf("{\"credential_present\":"+present+",\"internet\":"+internet+",\"private_directory\":"+quote(root==null?"":root.getAbsolutePath())+",\"error\":"+quote(error)+"}");
 }catch(Throwable ignored){return utf("{\"credential_present\":false,\"error\":\"credential_storage\"}");}finally{if(value!=null)java.util.Arrays.fill(value,(byte)0);}}
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
        if(root!=null){new File(root,"credential.enc").delete();new File(root,"credential.tmp").delete();FileOutputStream marker=new FileOutputStream(new File(root,"credential.removed"));try{marker.write(1);marker.getFD().sync();}finally{marker.close();}credentialBlocked=true;}
    }

 public static void showImport(android.app.Activity activity){NativeEditor.showSecret(activity);}
 public static void importAsync(final byte[] secret){Thread t=new Thread(new Runnable(){public void run(){control("import_key",secret);}},"NPC-AI-Eleven-key");t.setDaemon(true);t.start();}
 public static int start(String voice,byte[] payload,int timeout){
  if(!internet || voice==null || (!voice.isEmpty() && !voice.matches("[A-Za-z0-9_-]{1,128}")) || payload==null || payload.length>16384 || timeout<1 || timeout>60000 || !slots.tryAcquire())return -1;
  int id=sequence.incrementAndGet();Job j=new Job(voice,payload,timeout);jobs.put(id,j);Thread t=new Thread(j,"NPC-AI-ElevenLabs");t.setDaemon(true);t.start();return id;
 }
 public static byte[] poll(int id){Job j=jobs.get(id);return j==null?info(0,"cancelled",""):j.result;}
 public static byte[] audio(int id){Job j=jobs.remove(id);return j==null?null:j.audio;}
 public static void cancel(int id){final Job j=jobs.remove(id);if(j==null)return;j.cancelled=true;Thread t=new Thread(new Runnable(){public void run(){HttpsURLConnection c=j.connection;if(c!=null)c.disconnect();}},"NPC-AI-Eleven-close");t.setDaemon(true);t.start();}
 private static final class Job implements Runnable {
  final String voice;final byte[] payload;final int timeout;volatile boolean cancelled;volatile HttpsURLConnection connection;volatile byte[] result,audio;
  Job(String v,byte[] p,int t){voice=v;payload=p;timeout=t;}
  public void run(){long deadline=System.nanoTime()+timeout*1000000L;try{
   String key=credential();if(key.isEmpty()){result=info(0,"credential_missing","");return;}
   boolean catalog=voice.isEmpty();URL url=new URL(catalog?"https://api.elevenlabs.io/v2/voices?page_size=100":"https://api.elevenlabs.io/v1/text-to-speech/"+voice+"?output_format=pcm_24000");
   connection=(HttpsURLConnection)url.openConnection();connection.setInstanceFollowRedirects(false);connection.setConnectTimeout(Math.min(timeout,8000));connection.setReadTimeout(timeout);connection.setRequestProperty("xi-api-key",key);
   if(!catalog){connection.setRequestMethod("POST");connection.setDoOutput(true);connection.setRequestProperty("Content-Type","application/json");connection.setRequestProperty("Accept","audio/pcm");connection.setFixedLengthStreamingMode(payload.length);if(cancelled)return;OutputStream out=connection.getOutputStream();try{out.write(payload);}finally{out.close();}}
   if(cancelled)return;int code=connection.getResponseCode();if(code!=200){result=info(code,"provider","");return;}
   String type=connection.getContentType();if(!catalog && (type==null || !(type.startsWith("audio/pcm") || type.startsWith("audio/raw") || type.startsWith("application/octet-stream")))){result=info(0,"audio_format","");return;}
   InputStream in=connection.getInputStream();ByteArrayOutputStream out=new ByteArrayOutputStream();try{byte[] b=new byte[8192];int n;while(!cancelled){long remaining=(deadline-System.nanoTime())/1000000;if(remaining<=0)throw new SocketTimeoutException();connection.setReadTimeout((int)Math.min(60000,remaining));n=in.read(b);if(n<0)break;if(out.size()+n>(catalog?524288:1920000))throw new IOException();out.write(b,0,n);}}finally{in.close();}
   if(!cancelled){byte[] bytes=out.toByteArray();if(catalog)result=info(200,"",str(bytes));else if(bytes.length>0 && bytes.length%2==0){audio=bytes;result=info(200,"","");}else result=info(0,"audio_format","");}
  }catch(SSLException e){result=info(0,"tls","");}catch(SocketTimeoutException e){result=info(0,"timeout","");}catch(GeneralSecurityException e){result=info(0,"credential_missing","");}catch(Throwable e){result=info(0,"network","");}finally{if(connection!=null)connection.disconnect();slots.release();}}
 }
}
