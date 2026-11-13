package br.com.npcai;
import android.media.AudioTrack;
import android.media.AudioFormat;
import android.media.AudioManager;
/** PCM only. Called by the dedicated native voice worker, never the GTA render loop. */
public final class NativeAudio {
    private static volatile AudioTrack track;
    public static String hash(String path){
        try{java.security.MessageDigest digest=java.security.MessageDigest.getInstance("SHA-256");java.io.File file=new java.io.File(path);if(!file.isFile() || file.length()>536870912)return "";java.io.FileInputStream in=new java.io.FileInputStream(file);try{byte[] b=new byte[32768];int n;while((n=in.read(b))>=0)digest.update(b,0,n);}finally{in.close();}StringBuilder out=new StringBuilder();for(byte b:digest.digest())out.append(String.format(java.util.Locale.US,"%02x",b&255));return out.toString();}catch(Throwable ignored){return "";}
    }
    public static boolean open(int rate,float volume){
        close();
        if(rate<8000 || rate>48000 || Float.isNaN(volume) || volume<0 || volume>1)return false;
        try{
            int minimum=AudioTrack.getMinBufferSize(rate,AudioFormat.CHANNEL_OUT_STEREO,AudioFormat.ENCODING_PCM_16BIT);
            if(minimum<=0 || minimum>rate*4)return false;
            track=new AudioTrack(AudioManager.STREAM_MUSIC,rate,AudioFormat.CHANNEL_OUT_STEREO,AudioFormat.ENCODING_PCM_16BIT,Math.max(minimum,rate*2/5),AudioTrack.MODE_STREAM);
            if(track.getState()!=AudioTrack.STATE_INITIALIZED){close();return false;}
            track.setStereoVolume(volume,volume);track.play();return true;
        }catch(Throwable ignored){close();return false;}
    }
    public static void gains(float left,float right){try{if(track!=null && !Float.isNaN(left) && !Float.isNaN(right))track.setStereoVolume(Math.max(0,Math.min(1,left)),Math.max(0,Math.min(1,right)));}catch(Throwable ignored){}}
    public static int write(short[] samples){try{AudioTrack current=track;return current==null?-1:current.write(samples,0,samples.length);}catch(Throwable ignored){return -1;}}
    public static long head(){try{AudioTrack current=track;return current==null?-1:((long)current.getPlaybackHeadPosition() & 0xffffffffL);}catch(Throwable ignored){return -1;}}
    public static void close(){AudioTrack old=track;track=null;if(old!=null){try{old.pause();old.flush();}catch(Throwable ignored){}try{old.release();}catch(Throwable ignored){}}}
}
