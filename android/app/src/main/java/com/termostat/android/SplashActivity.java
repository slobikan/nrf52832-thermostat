package com.termostat.android;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;

import androidx.appcompat.app.AppCompatActivity;

import com.termostat.android.databinding.ActivitySplashBinding;

public class SplashActivity extends AppCompatActivity {
    private static final long SPLASH_DELAY_MS = 1500L;

    private final Handler handler = new Handler(Looper.getMainLooper());
    private final Runnable openMain = new Runnable() {
        @Override
        public void run() {
            startActivity(new Intent(SplashActivity.this, MainActivity.class));
            overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out);
            finish();
        }
    };

    @Override
    protected void attachBaseContext(Context newBase) {
        super.attachBaseContext(LocaleHelper.wrap(newBase));
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        ActivitySplashBinding binding = ActivitySplashBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        binding.splashLogoBadge.setScaleX(0.76f);
        binding.splashLogoBadge.setScaleY(0.76f);
        binding.splashLogoBadge.setAlpha(0f);
        binding.splashTitle.setAlpha(0f);
        binding.splashSubtitle.setAlpha(0f);
        binding.splashLoading.setAlpha(0f);

        binding.splashLogoBadge.animate().alpha(1f).scaleX(1f).scaleY(1f).setDuration(650L).start();
        binding.splashTitle.animate().alpha(1f).setStartDelay(180L).setDuration(500L).start();
        binding.splashSubtitle.animate().alpha(1f).setStartDelay(320L).setDuration(500L).start();
        binding.splashLoading.animate().alpha(1f).setStartDelay(460L).setDuration(500L).start();

        handler.postDelayed(openMain, SPLASH_DELAY_MS);
    }

    @Override
    protected void onDestroy() {
        handler.removeCallbacks(openMain);
        super.onDestroy();
    }
}
