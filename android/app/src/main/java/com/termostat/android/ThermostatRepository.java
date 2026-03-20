package com.termostat.android;

import android.os.Handler;
import android.os.Looper;

import org.json.JSONException;

import java.io.IOException;
import java.util.Random;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class ThermostatRepository {
    public interface Callback {
        void onSuccess(ThermostatState state);

        void onError(String message);
    }

    private final GatewayPreferences preferences;
    private final ThermostatApiClient apiClient;
    private final ExecutorService executor;
    private final Handler mainHandler;
    private final Random random;

    public ThermostatRepository(GatewayPreferences preferences) {
        this.preferences = preferences;
        this.apiClient = new ThermostatApiClient();
        this.executor = Executors.newSingleThreadExecutor();
        this.mainHandler = new Handler(Looper.getMainLooper());
        this.random = new Random();
    }

    public void fetchState(String baseUrl, boolean demoMode, Callback callback) {
        executor.execute(() -> {
            if (demoMode) {
                deliverSuccess(callback, simulateDemoTick());
                return;
            }

            if (baseUrl == null || baseUrl.trim().isEmpty()) {
                deliverError(callback, "Enter a gateway URL such as http://192.168.4.1");
                return;
            }

            try {
                deliverSuccess(callback, apiClient.fetchState(baseUrl));
            } catch (IOException | JSONException error) {
                deliverError(callback, "Gateway sync failed: " + error.getMessage());
            }
        });
    }

    public void updateSetpoint(String baseUrl, boolean demoMode, int setpoint, Callback callback) {
        executor.execute(() -> {
            int clamped = clamp(setpoint, 5, 35);

            if (demoMode) {
                ThermostatState current = preferences.loadDemoState();
                ThermostatState updated = new ThermostatState(
                        current.currentTemperature,
                        clamped,
                        current.currentTemperature < clamped,
                        current.paired,
                        true,
                        "",
                        System.currentTimeMillis()
                );
                preferences.saveDemoState(updated);
                deliverSuccess(callback, updated);
                return;
            }

            if (baseUrl == null || baseUrl.trim().isEmpty()) {
                deliverError(callback, "Set a gateway URL before applying target.");
                return;
            }

            try {
                deliverSuccess(callback, apiClient.updateSetpoint(baseUrl, clamped));
            } catch (IOException | JSONException error) {
                deliverError(callback, "Target update failed: " + error.getMessage());
            }
        });
    }

    public void requestPair(String baseUrl, boolean demoMode, Callback callback) {
        executor.execute(() -> {
            if (demoMode) {
                ThermostatState current = preferences.loadDemoState();
                ThermostatState updated = new ThermostatState(
                        current.currentTemperature,
                        current.setpoint,
                        current.heatingOn,
                        true,
                        true,
                        "",
                        System.currentTimeMillis()
                );
                preferences.saveDemoState(updated);
                deliverSuccess(callback, updated);
                return;
            }

            if (baseUrl == null || baseUrl.trim().isEmpty()) {
                deliverError(callback, "Set a gateway URL before pairing.");
                return;
            }

            try {
                deliverSuccess(callback, apiClient.requestPair(baseUrl));
            } catch (IOException | JSONException error) {
                deliverError(callback, "Pair command failed: " + error.getMessage());
            }
        });
    }

    public void shutdown() {
        executor.shutdownNow();
    }

    private ThermostatState simulateDemoTick() {
        ThermostatState previous = preferences.loadDemoState();
        int currentTemperature = previous.currentTemperature;

        if (previous.heatingOn && currentTemperature < previous.setpoint + 1) {
            currentTemperature += 1;
        } else {
            currentTemperature += random.nextInt(3) - 1;
        }

        currentTemperature = clamp(currentTemperature, 15, 30);
        boolean heatingOn = currentTemperature < previous.setpoint;

        ThermostatState updated = new ThermostatState(
                currentTemperature,
                previous.setpoint,
                heatingOn,
                previous.paired,
                true,
                "",
                System.currentTimeMillis()
        );
        preferences.saveDemoState(updated);
        return updated;
    }

    private void deliverSuccess(Callback callback, ThermostatState state) {
        mainHandler.post(() -> callback.onSuccess(state));
    }

    private void deliverError(Callback callback, String message) {
        mainHandler.post(() -> callback.onError(message));
    }

    private int clamp(int value, int min, int max) {
        return Math.max(min, Math.min(max, value));
    }
}
