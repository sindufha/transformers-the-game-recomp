package io.github.transformers.recompiled.diagnostics;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

/** Keeps the existing native session eligible for background resume. A killed
 * process is not restarted into a nonexistent session; removing the task or
 * explicitly choosing Exit ends it. No wake lock or background rendering. */
public final class GameSessionService extends Service {
    @Override public void onCreate(){
        super.onCreate();
        String channel="game-session";
        NotificationManager manager=getSystemService(NotificationManager.class);
        manager.createNotificationChannel(new NotificationChannel(channel,"Game session",NotificationManager.IMPORTANCE_LOW));
        Intent resume=new Intent(this,GameActivity.class).addFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT|Intent.FLAG_ACTIVITY_SINGLE_TOP);
        PendingIntent action=PendingIntent.getActivity(this,0,resume,PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE);
        Notification notification=new Notification.Builder(this,channel)
            .setSmallIcon(android.R.drawable.ic_media_play).setContentTitle("Transformers: The Game")
            .setContentText("Game session active. Tap to return to the game.")
            .setContentIntent(action).setOngoing(true).setOnlyAlertOnce(true)
            .setCategory(Notification.CATEGORY_SERVICE).build();
        startForeground(1,notification);
    }
    @Override public int onStartCommand(Intent intent,int flags,int startId){return START_NOT_STICKY;}
    @Override public IBinder onBind(Intent intent){return null;}
    @Override public void onTaskRemoved(Intent rootIntent){
        stopForeground(true);stopSelf();android.os.Process.killProcess(android.os.Process.myPid());
    }
}
