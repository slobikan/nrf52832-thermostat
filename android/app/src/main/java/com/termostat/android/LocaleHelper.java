package com.termostat.android;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Configuration;

import java.util.Locale;

public final class LocaleHelper {
    private LocaleHelper() {
    }

    public static Context wrap(Context context) {
        SharedPreferences preferences = context.getSharedPreferences(GatewayPreferences.PREFS_NAME, Context.MODE_PRIVATE);
        String language = preferences.getString(GatewayPreferences.KEY_LANGUAGE, "pl");
        Locale locale = new Locale(language);
        Locale.setDefault(locale);

        Configuration configuration = new Configuration(context.getResources().getConfiguration());
        configuration.setLocale(locale);
        configuration.setLayoutDirection(locale);
        return context.createConfigurationContext(configuration);
    }
}
