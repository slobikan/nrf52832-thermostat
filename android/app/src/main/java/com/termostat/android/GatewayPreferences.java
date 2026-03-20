package com.termostat.android;

import android.content.Context;
import android.content.SharedPreferences;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public class GatewayPreferences {
    public static final int MAX_ZONES = 15;
    public static final String PREFS_NAME = "thermostat_android";
    public static final String KEY_LANGUAGE = "language";
    public static final String KEY_MODE = "work_mode";

    private static final String KEY_BASE_URL = "base_url";
    private static final String KEY_DEMO_MODE = "demo_mode";
    private static final String KEY_DEMO_TEMP = "demo_temp";
    private static final String KEY_DEMO_SETPOINT = "demo_setpoint";
    private static final String KEY_DEMO_PAIRED = "demo_paired";
    private static final String KEY_ZONE_COUNT = "zone_count";
    private static final String KEY_VISIBLE_ZONES = "visible_zones";
    private static final String KEY_ZONE_SETPOINT_PREFIX = "zone_setpoint_";
    private static final String KEY_WIFI_SSID = "wifi_ssid";
    private static final String KEY_WIFI_PASSWORD = "wifi_password";
    private static final String KEY_WIFI_DHCP = "wifi_dhcp";
    private static final String KEY_WIFI_IP = "wifi_ip";

    private final SharedPreferences preferences;

    public GatewayPreferences(Context context) {
        preferences = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
    }

    public String loadBaseUrl() {
        return preferences.getString(KEY_BASE_URL, "http://192.168.4.1");
    }

    public void saveBaseUrl(String baseUrl) {
        preferences.edit().putString(KEY_BASE_URL, normalize(baseUrl)).apply();
    }

    public boolean isDemoMode() {
        return preferences.getBoolean(KEY_DEMO_MODE, true);
    }

    public void setDemoMode(boolean enabled) {
        preferences.edit().putBoolean(KEY_DEMO_MODE, enabled).apply();
    }

    public int getZoneCount() {
        return preferences.getInt(KEY_ZONE_COUNT, 1);
    }

    public void setZoneCount(int count) {
        int normalized = Math.max(1, Math.min(MAX_ZONES, count));
        preferences.edit().putInt(KEY_ZONE_COUNT, normalized).apply();
    }

    public List<Integer> getVisibleZones() {
        Set<String> stored = preferences.getStringSet(KEY_VISIBLE_ZONES, null);
        List<Integer> zones = new ArrayList<>();

        if (stored == null) {
            int fallbackCount = Math.max(1, Math.min(MAX_ZONES, getZoneCount()));
            for (int i = 1; i <= fallbackCount; i++) {
                zones.add(i);
            }
            return zones;
        }

        for (String value : stored) {
            try {
                int zone = Integer.parseInt(value);
                if (zone >= 1 && zone <= MAX_ZONES) {
                    zones.add(zone);
                }
            } catch (NumberFormatException ignored) {
            }
        }

        Collections.sort(zones);
        return zones;
    }

    public boolean isZoneVisible(int zoneIndex) {
        return getVisibleZones().contains(zoneIndex);
    }

    public void setZoneVisible(int zoneIndex, boolean visible) {
        if (zoneIndex < 1 || zoneIndex > MAX_ZONES) {
            return;
        }

        Set<String> updated = new HashSet<>();
        for (int zone : getVisibleZones()) {
            updated.add(String.valueOf(zone));
        }

        if (visible) {
            updated.add(String.valueOf(zoneIndex));
        } else {
            updated.remove(String.valueOf(zoneIndex));
        }

        preferences.edit().putStringSet(KEY_VISIBLE_ZONES, updated).apply();
    }

    public float getZoneSetpoint(int zoneIndex, float fallback) {
        if (zoneIndex < 1 || zoneIndex > MAX_ZONES) {
            return fallback;
        }
        return preferences.getFloat(KEY_ZONE_SETPOINT_PREFIX + zoneIndex, fallback);
    }

    public void setZoneSetpoint(int zoneIndex, float value) {
        if (zoneIndex < 1 || zoneIndex > MAX_ZONES) {
            return;
        }
        float normalized = Math.max(5f, Math.min(35f, value));
        preferences.edit().putFloat(KEY_ZONE_SETPOINT_PREFIX + zoneIndex, normalized).apply();
    }

    public String getMode() {
        return preferences.getString(KEY_MODE, "auto");
    }

    public void setMode(String mode) {
        preferences.edit().putString(KEY_MODE, mode).apply();
    }

    public String getLanguage() {
        return preferences.getString(KEY_LANGUAGE, "pl");
    }

    public void setLanguage(String language) {
        preferences.edit().putString(KEY_LANGUAGE, language).apply();
    }

    public String getWifiSsid() {
        return preferences.getString(KEY_WIFI_SSID, "");
    }

    public void setWifiSsid(String value) {
        preferences.edit().putString(KEY_WIFI_SSID, normalize(value)).apply();
    }

    public String getWifiPassword() {
        return preferences.getString(KEY_WIFI_PASSWORD, "");
    }

    public void setWifiPassword(String value) {
        preferences.edit().putString(KEY_WIFI_PASSWORD, value == null ? "" : value).apply();
    }

    public boolean isWifiDhcp() {
        return preferences.getBoolean(KEY_WIFI_DHCP, true);
    }

    public void setWifiDhcp(boolean enabled) {
        preferences.edit().putBoolean(KEY_WIFI_DHCP, enabled).apply();
    }

    public String getWifiIp() {
        return preferences.getString(KEY_WIFI_IP, "192.168.4.1");
    }

    public void setWifiIp(String value) {
        preferences.edit().putString(KEY_WIFI_IP, normalize(value)).apply();
    }

    public String getScheduleStart(int zoneIndex, int slotIndex) {
        return preferences.getString(scheduleKey(zoneIndex, slotIndex, true), "00:00");
    }

    public String getScheduleEnd(int zoneIndex, int slotIndex) {
        return preferences.getString(scheduleKey(zoneIndex, slotIndex, false), "23:59");
    }

    public void setScheduleStart(int zoneIndex, int slotIndex, String value) {
        preferences.edit().putString(scheduleKey(zoneIndex, slotIndex, true), value).apply();
    }

    public void setScheduleEnd(int zoneIndex, int slotIndex, String value) {
        preferences.edit().putString(scheduleKey(zoneIndex, slotIndex, false), value).apply();
    }

    public ThermostatState loadDemoState() {
        int temperature = preferences.getInt(KEY_DEMO_TEMP, 23);
        int setpoint = preferences.getInt(KEY_DEMO_SETPOINT, 22);
        boolean paired = preferences.getBoolean(KEY_DEMO_PAIRED, false);
        return new ThermostatState(
                temperature,
                setpoint,
                temperature < setpoint,
                paired,
                true,
                "",
                System.currentTimeMillis()
        );
    }

    public void saveDemoState(ThermostatState state) {
        preferences.edit()
                .putInt(KEY_DEMO_TEMP, state.currentTemperature)
                .putInt(KEY_DEMO_SETPOINT, state.setpoint)
                .putBoolean(KEY_DEMO_PAIRED, state.paired)
                .apply();
    }

    private String scheduleKey(int zoneIndex, int slotIndex, boolean start) {
        return "zone_" + zoneIndex + "_slot_" + slotIndex + (start ? "_start" : "_end");
    }

    private String normalize(String value) {
        return value == null ? "" : value.trim();
    }
}
