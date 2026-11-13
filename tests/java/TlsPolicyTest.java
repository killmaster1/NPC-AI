import javax.net.ssl.*;
import java.net.*;
public class TlsPolicyTest {
    public static void main(String[] args)throws Exception {
        HttpsURLConnection c=(HttpsURLConnection)new URL(args[0]).openConnection();c.setConnectTimeout(2000);c.setReadTimeout(2000);
        boolean rejected=false;try{c.getInputStream();}catch(SSLHandshakeException e){rejected=true;}finally{c.disconnect();}
        if(!rejected)throw new AssertionError("untrusted TLS accepted");System.out.println("TLS self-signed certificate rejected by default verifier");
    }
}
