package com.termostat.android;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;

public class ThermostatApiClient {
    private static final int CONNECT_TIMEOUT_MS = 2500;
    private static final int READ_TIMEOUT_MS = 2500;

    public ThermostatState fetchState(String baseUrl) throws IOException, JSONException {
        String body = executeRequest(baseUrl + "/api/thermostat", "GET", null);
        return parseState(body, false);
    }

    public ThermostatState updateSetpoint(String baseUrl, int setpoint) throws IOException, JSONException {
        JSONObject payload = new JSONObject();
        payload.put("setpoint", setpoint);
        String body = executeRequest(baseUrl + "/api/setpoint", "POST", payload.toString());
        if (body.trim().isEmpty()) {
            return fetchState(baseUrl);
        }
        return parseState(body, false);
    }

    public ThermostatState requestPair(String baseUrl) throws IOException, JSONException {
        String body = executeRequest(baseUrl + "/api/pair", "POST", "{}");
        if (body.trim().isEmpty()) {
            return fetchState(baseUrl);
        }
        return parseState(body, false);
    }

    private String executeRequest(String url, String method, String requestBody) throws IOException {
        HttpURLConnection connection = (HttpURLConnection) new URL(url).openConnection();
        connection.setRequestMethod(method);
        connection.setConnectTimeout(CONNECT_TIMEOUT_MS);
        connection.setReadTimeout(READ_TIMEOUT_MS);
        connection.setRequestProperty("Accept", "application/json");

        if (requestBody != null) {
            connection.setDoOutput(true);
            connection.setRequestProperty("Content-Type", "application/json; charset=utf-8");
            byte[] payload = requestBody.getBytes(StandardCharsets.UTF_8);
            try (OutputStream outputStream = connection.getOutputStream()) {
                outputStream.write(payload);
            }
        }

        int responseCode = connection.getResponseCode();
        InputStream stream = responseCode >= 400 ? connection.getErrorStream() : connection.getInputStream();
        String body = readFully(stream);
        if (responseCode >= 400) {
            throw new IOException(body.isEmpty() ? "HTTP " + responseCode : body);
        }
        return body;
    }

    private ThermostatState parseState(String body, boolean demoMode) throws JSONException {
        JSONObject root = new JSONObject(body);
        JSONObject payload = root.optJSONObject("state") != null ? root.optJSONObject("state") : root;

        int currentTemperature = payload.has("temperature")
                ? payload.optInt("temperature", 23)
                : payload.optInt("temp", 23);
        int setpoint = payload.optInt("setpoint", 22);
        boolean heatingOn = payload.has("heating")
                ? payload.optBoolean("heating", currentTemperature < setpoint)
                : payload.optBoolean("heatingOn", currentTemperature < setpoint);
        boolean paired = payload.has("paired")
                ? payload.optBoolean("paired", false)
                : payload.optBoolean("linked", false);
        String statusMessage = payload.optString("status", "");

        return new ThermostatState(
                currentTemperature,
                setpoint,
                heatingOn,
                paired,
                demoMode,
                statusMessage,
                System.currentTimeMillis()
        );
    }

    private String readFully(InputStream stream) throws IOException {
        if (stream == null) {
            return "";
        }

        StringBuilder builder = new StringBuilder();
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(stream, StandardCharsets.UTF_8))) {
            String line;
            while ((line = reader.readLine()) != null) {
                builder.append(line);
            }
        }
        return builder.toString();
    }
}
