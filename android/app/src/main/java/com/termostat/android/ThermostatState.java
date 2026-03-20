package com.termostat.android;

public class ThermostatState {
    public final int currentTemperature;
    public final int setpoint;
    public final boolean heatingOn;
    public final boolean paired;
    public final boolean demoMode;
    public final String statusMessage;
    public final long updatedAtMillis;

    public ThermostatState(
            int currentTemperature,
            int setpoint,
            boolean heatingOn,
            boolean paired,
            boolean demoMode,
            String statusMessage,
            long updatedAtMillis
    ) {
        this.currentTemperature = currentTemperature;
        this.setpoint = setpoint;
        this.heatingOn = heatingOn;
        this.paired = paired;
        this.demoMode = demoMode;
        this.statusMessage = statusMessage;
        this.updatedAtMillis = updatedAtMillis;
    }
}
