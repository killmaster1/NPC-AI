package br.com.npcai;

import android.app.Activity;
import android.app.Application;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.graphics.Color;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.InputFilter;
import android.text.InputType;
import android.text.Spanned;
import android.util.Log;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.lang.ref.WeakReference;
import java.nio.charset.Charset;
import java.lang.reflect.Method;

/** Uses only the host Activity's existing content/window. Never creates a Dialog. */
public final class NativeEditor {
    private static final String TAG="NPC_AI_IME";
    private static final Charset UTF8=Charset.forName("UTF-8");
    private static final Handler MAIN=new Handler(Looper.getMainLooper());
    private static Session active;
    private static Method rootInsets,imeMask,insetsVisible;
    private static boolean insetsUnsupported;
    // API30+: observe visibility even for floating IMEs that do not resize the window.
    private static Boolean imeVisible(View view){
        if(insetsUnsupported)return null;
        try{
            if(rootInsets==null){
                rootInsets=View.class.getMethod("getRootWindowInsets");
                imeMask=Class.forName("android.view.WindowInsets$Type").getMethod("ime");
                insetsVisible=Class.forName("android.view.WindowInsets").getMethod("isVisible",Integer.TYPE);
            }
            Object insets=rootInsets.invoke(view);
            return insets==null?null:(Boolean)insetsVisible.invoke(insets,imeMask.invoke(null));
        }catch(Exception error){insetsUnsupported=true;log("ime_insets_unavailable_using_visible_frame");return null;}
    }
    private static long closedThrough;
    private static Application observedApp;
    private static WeakReference<Activity> observedActivity=new WeakReference<Activity>(null);
    private static native void finish(long ticket,byte[] text);
    private static void log(String event){Log.i(TAG,event);}
    public static String version(Context context) throws Exception {
        PackageInfo info=context.getPackageManager().getPackageInfo(context.getPackageName(),0);
        return info.versionName;
    }
    private static void observe(Activity activity){
        observedActivity=new WeakReference<Activity>(activity);
        if(observedApp==activity.getApplication()) return;
        observedApp=activity.getApplication();
        observedApp.registerActivityLifecycleCallbacks(new Application.ActivityLifecycleCallbacks(){
            private boolean host(Activity a){return a==observedActivity.get();}
            public void onActivityCreated(Activity a,Bundle b){if(host(a))log("activity_created");}
            public void onActivityStarted(Activity a){if(host(a))log("activity_started");}
            public void onActivityResumed(Activity a){if(host(a))log("activity_resumed window_focus="+a.hasWindowFocus());}
            public void onActivityPaused(Activity a){if(host(a)){log("activity_paused");if(active!=null)active.end(null,"activity_pause",true);}}
            public void onActivityStopped(Activity a){if(host(a))log("activity_stopped");}
            public void onActivitySaveInstanceState(Activity a,Bundle b){if(host(a))log("activity_save_state");}
            public void onActivityDestroyed(Activity a){if(host(a)){log("activity_destroyed");if(active!=null)active.end(null,"activity_destroy",true);}}
        });
    }
    public static void show(final Activity activity,final byte[] initial,final byte[] history,final long ticket){
        MAIN.post(new Runnable(){public void run(){
            if(ticket<=closedThrough){finish(ticket,null);return;}
            if(active!=null)active.end(null,"replaced",true);
            try{
                if(activity.isFinishing()){finish(ticket,null);return;}
                observe(activity);
                active=new Session(activity,ticket);
                active.open(initial,history);
            }catch(Exception error){
                log("open_failed class="+error.getClass().getSimpleName());
                if(active!=null)active.end(null,"open_failed",true);else finish(ticket,null);
            }
        }});
    }
    public static void showSecret(final Activity activity){MAIN.post(new Runnable(){public void run(){
        if(active!=null)active.end(null,"credential_replaced",true);
        try{if(activity==null || activity.isFinishing())return;observe(activity);active=new Session(activity,0,true);active.open(new byte[0],"Chave ElevenLabs: será guardada separadamente no Android Keystore. Não será enviada ao Gemini.".getBytes(UTF8));}
        catch(Throwable ignored){if(active!=null)active.end(null,"credential_failed",true);}
    }});}
    /** Called from C++ after invalidation. UI work remains on MAIN. */
    public static void hide(final long ticket){
        MAIN.post(new Runnable(){public void run(){
            closedThrough=Math.max(closedThrough,ticket);
            if(active!=null && active.ticket==ticket)active.end(null,"native_invalidation",false);
        }});
    }
    private static final class Session implements ViewTreeObserver.OnGlobalLayoutListener {
        final Activity activity;
        final long ticket;final boolean secret;
        ViewGroup content;
        FrameLayout overlay;
        LinearLayout panel;
        ScrollView historyView;
        EditText field;
        View previousFocus;
        int previousSoftInputMode;
        boolean changedMode,closed,seenKeyboard,lastKeyboard;
        final Rect visible=new Rect();
        final Runnable imeWatcher=new Runnable(){public void run(){
            if(closed)return;onGlobalLayout();MAIN.postDelayed(this,200);
        }};
        Session(Activity a,long t){this(a,t,false);}Session(Activity a,long t,boolean sensitive){activity=a;ticket=t;secret=sensitive;}
        int dp(int value){return (int)(value*activity.getResources().getDisplayMetrics().density+.5f);}
        void open(byte[] initial,byte[] history){
            View host=activity.findViewById(android.R.id.content);
            if(!(host instanceof ViewGroup))throw new IllegalStateException("Missing content ViewGroup");
            content=(ViewGroup)host;previousFocus=activity.getCurrentFocus();
            previousSoftInputMode=activity.getWindow().getAttributes().softInputMode;
            activity.getWindow().setSoftInputMode((previousSoftInputMode & ~WindowManager.LayoutParams.SOFT_INPUT_MASK_ADJUST)|WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
            changedMode=true;
            overlay=new FrameLayout(activity){
                public void onWindowFocusChanged(boolean focused){
                    super.onWindowFocusChanged(focused);
                    log("window_focus="+focused+" activity_focus="+activity.hasWindowFocus());
                }
            };
            overlay.setBackgroundColor(0x44000000);
            // Consume all touches outside the composer; children receive their own touches normally.
            overlay.setClickable(true);overlay.setFocusableInTouchMode(true);
            panel=new LinearLayout(activity);panel.setOrientation(LinearLayout.VERTICAL);
            panel.setPadding(dp(10),dp(6),dp(10),dp(6));panel.setBackgroundColor(Color.rgb(28,31,38));panel.setClickable(true);
            LinearLayout title=new LinearLayout(activity);title.setGravity(Gravity.CENTER_VERTICAL);
            TextView label=new TextView(activity);label.setText(secret?"Importar chave ElevenLabs":"Conversa com NPC");label.setTextColor(Color.WHITE);
            title.addView(label,new LinearLayout.LayoutParams(0,-2,1));
            Button cancel=new Button(activity);cancel.setText("Fechar");title.addView(cancel);panel.addView(title);
            historyView=new ScrollView(activity);TextView transcript=new TextView(activity);
            transcript.setText(new String(history,UTF8));transcript.setTextColor(Color.rgb(220,225,240));transcript.setTextSize(15);
            historyView.addView(transcript);panel.addView(historyView,new LinearLayout.LayoutParams(-1,dp(90)));
            LinearLayout composer=new LinearLayout(activity);composer.setGravity(Gravity.BOTTOM);
            field=new EditText(activity){
                public android.view.inputmethod.InputConnection onCreateInputConnection(EditorInfo info){
                    android.view.inputmethod.InputConnection connection=super.onCreateInputConnection(info);
                    info.imeOptions=(info.imeOptions & ~EditorInfo.IME_MASK_ACTION & ~EditorInfo.IME_FLAG_NO_ENTER_ACTION)|EditorInfo.IME_ACTION_SEND;
                    return connection;
                }
                public boolean onKeyPreIme(int keyCode,KeyEvent event){
                    if(keyCode==KeyEvent.KEYCODE_BACK){
                        if(event.getAction()==KeyEvent.ACTION_UP)end(null,"back_pre_ime",true);
                        return true;
                    }
                    return super.onKeyPreIme(keyCode,event);
                }
            };
            field.setTextColor(Color.WHITE);field.setHintTextColor(Color.LTGRAY);field.setHint("Mensagem para o NPC...");
            field.setInputType(InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_FLAG_CAP_SENTENCES|InputType.TYPE_TEXT_FLAG_MULTI_LINE);
            if(secret){field.setHint("Chave ElevenLabs");field.setInputType(InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_VARIATION_PASSWORD|InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);}
            field.setMinLines(1);field.setMaxLines(3);field.setImeOptions(EditorInfo.IME_ACTION_SEND);
            field.setFilters(new InputFilter[]{new InputFilter(){public CharSequence filter(CharSequence src,int start,int end,Spanned dst,int dstart,int dend){
                String next=dst.subSequence(0,dstart).toString()+src.subSequence(start,end)+dst.subSequence(dend,dst.length());
                return next.getBytes(UTF8).length<=(secret?512:1200)?null:"";
            }}});
            field.setText(new String(initial,UTF8));field.setSelection(field.length());
            composer.addView(field,new LinearLayout.LayoutParams(0,-2,1));
            Button send=new Button(activity);send.setText("Enviar");send.setMinWidth(dp(80));send.setMinHeight(dp(48));composer.addView(send);panel.addView(composer);
            send.setOnClickListener(new View.OnClickListener(){public void onClick(View view){submit("send_button");}});
            field.setOnEditorActionListener(new TextView.OnEditorActionListener(){public boolean onEditorAction(TextView v,int action,KeyEvent event){
                if(action==EditorInfo.IME_ACTION_SEND){submit("ime_send");return true;}return false;
            }});
            field.setOnFocusChangeListener(new View.OnFocusChangeListener(){public void onFocusChange(View v,boolean has){log("edit_focus="+has);}});
            cancel.setOnClickListener(new View.OnClickListener(){public void onClick(View v){end(null,"close_button",true);}});
            overlay.setOnKeyListener(new View.OnKeyListener(){public boolean onKey(View v,int key,KeyEvent e){
                if(key==KeyEvent.KEYCODE_BACK){if(e.getAction()==KeyEvent.ACTION_UP)end(null,"back",true);return true;}return false;
            }});
            FrameLayout.LayoutParams bottom=new FrameLayout.LayoutParams(-1,-2,Gravity.BOTTOM);overlay.addView(panel,bottom);
            content.addView(overlay,new ViewGroup.LayoutParams(-1,-1));overlay.bringToFront();
            overlay.getViewTreeObserver().addOnGlobalLayoutListener(this);
            log("editor_attached same_activity=true window_focus="+activity.hasWindowFocus());
            MAIN.post(imeWatcher);
            field.requestFocus();
            field.post(new Runnable(){public void run(){if(closed)return;
                InputMethodManager ime=(InputMethodManager)activity.getSystemService(Context.INPUT_METHOD_SERVICE);
                boolean requested=ime!=null && ime.showSoftInput(field,InputMethodManager.SHOW_IMPLICIT);
                log("ime_show_requested="+requested);historyView.fullScroll(View.FOCUS_DOWN);
            }});
        }
        void submit(String source){
            log(source);
            String text=field.getText().toString();byte[] bytes=text.getBytes(UTF8);
            if(text.trim().length()==0 || bytes.length>(secret?512:1200)){log("send_rejected_empty_or_limit");return;}
            end(bytes,"submit",true);
        }
        public void onGlobalLayout(){
            if(closed || overlay.getHeight()<=0)return;
            overlay.getWindowVisibleDisplayFrame(visible);
            int[] location=new int[2];overlay.getLocationOnScreen(location);
            int inset=Math.max(0,location[1]+overlay.getHeight()-visible.bottom);
            int available=Math.max(0,Math.min(overlay.getHeight(),visible.bottom-location[1]));
            FrameLayout.LayoutParams lp=(FrameLayout.LayoutParams)panel.getLayoutParams();
            int margin=Math.min(inset,Math.max(0,overlay.getHeight()));
            if(lp.bottomMargin!=margin){lp.bottomMargin=margin;panel.setLayoutParams(lp);}
            int historyHeight=Math.min(dp(100),Math.max(0,available-dp(180)));
            ViewGroup.LayoutParams hp=historyView.getLayoutParams();
            if(hp.height!=historyHeight){hp.height=historyHeight;historyView.setLayoutParams(hp);}
            int full=activity.getResources().getDisplayMetrics().heightPixels;
            boolean keyboard=full-visible.height()>Math.max(dp(100),full/5);
            Boolean platformVisible=imeVisible(overlay);if(platformVisible!=null)keyboard=platformVisible.booleanValue();
            if(keyboard!=lastKeyboard){log("ime_visible="+keyboard+" visible_height="+visible.height());lastKeyboard=keyboard;}
            if(keyboard)seenKeyboard=true;
            else if(seenKeyboard){
                seenKeyboard=false;
                MAIN.postDelayed(new Runnable(){public void run(){if(!closed && !lastKeyboard)end(null,"ime_hidden_back_or_gesture",true);}},150);
            }
        }
        void end(byte[] text,String reason,boolean notify){
            if(closed)return;closed=true;MAIN.removeCallbacks(imeWatcher);
            log("editor_close reason="+reason);
            try{
                if(field!=null){
                    InputMethodManager ime=(InputMethodManager)activity.getSystemService(Context.INPUT_METHOD_SERVICE);
                    if(ime!=null)ime.hideSoftInputFromWindow(field.getWindowToken(),0);
                    field.clearFocus();
                }
                if(overlay!=null){
                    if(overlay.getViewTreeObserver().isAlive())overlay.getViewTreeObserver().removeOnGlobalLayoutListener(this);
                    if(overlay.getParent() instanceof ViewGroup)((ViewGroup)overlay.getParent()).removeView(overlay);
                }
            }catch(Exception error){log("detach_failed class="+error.getClass().getSimpleName());}
            finally{
                try{if(changedMode)activity.getWindow().setSoftInputMode(previousSoftInputMode);}
                catch(Exception error){log("restore_window_failed class="+error.getClass().getSimpleName());}
                try{if(previousFocus!=null && previousFocus.isShown())previousFocus.requestFocus();}
                catch(Exception error){log("restore_focus_failed class="+error.getClass().getSimpleName());}
                if(active==this)active=null;
                closedThrough=Math.max(closedThrough,ticket);
                if(secret){if(field!=null)field.setText("");if(notify && text!=null)NativeEleven.importAsync(text);}else if(notify)finish(ticket,text);
            }
        }
    }
}
