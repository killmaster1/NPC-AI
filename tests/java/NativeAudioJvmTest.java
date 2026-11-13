import br.com.npcai.NativeAudio;
public final class NativeAudioJvmTest {
 public static void main(String[] args){
  if(!NativeAudio.hash(args[0]).equals(args[1]))throw new AssertionError("SHA256 mismatch");
  if(!NativeAudio.hash(args[0]+".missing").isEmpty())throw new AssertionError("missing file");
  if(NativeAudio.open(-1,.8f) || NativeAudio.open(22050,Float.NaN))throw new AssertionError("invalid audio settings");
  if(NativeAudio.write(new short[0])!=-1 || NativeAudio.head()!=-1)throw new AssertionError("closed track");
  NativeAudio.close();NativeAudio.close();
  System.out.println("PCM validation/hash/closed state passed; AudioTrack playback requires Android");
 }
}
