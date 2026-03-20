package com.termostat.android;

import android.app.TimePickerDialog;
import android.content.Context;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.ContextThemeWrapper;
import android.view.Gravity;
import android.view.MenuItem;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;
import androidx.core.view.ViewCompat;

import com.google.android.material.card.MaterialCardView;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.android.material.slider.Slider;
import com.termostat.android.databinding.ActivityMainBinding;

import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;

public class MainActivity extends AppCompatActivity {
    private static final int MIN_SETPOINT = 5;
    private static final int MAX_SETPOINT = 35;
    private static final long POLL_INTERVAL_MS = 5000L;

    private enum MenuScreen {
        ZONES,
        MODES,
        SETTINGS
    }

    private enum ActionState {
        IDLE,
        HEAT,
        COOL,
        FAN
    }

    private final Handler pollHandler = new Handler(Looper.getMainLooper());
    private final Runnable pollRunnable = new Runnable() {
        @Override
        public void run() {
            refreshState(false);
            pollHandler.postDelayed(this, POLL_INTERVAL_MS);
        }
    };

    private final String[] languageCodes = new String[] {"pl", "en", "uk"};

    private ActivityMainBinding binding;
    private GatewayPreferences preferences;
    private ThermostatRepository repository;
    private ThermostatState currentState;
    private boolean sliderTracking;
    private boolean languageReady;
    private int editingZone = 1;

    @Override
    protected void attachBaseContext(Context newBase) {
        super.attachBaseContext(LocaleHelper.wrap(newBase));
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        preferences = new GatewayPreferences(this);
        repository = new ThermostatRepository(preferences);

        configureUi();
        showHomeScreen();
        currentState = preferences.loadDemoState();
        renderState(currentState);
        refreshState(true);
    }

    @Override
    public void onBackPressed() {
        if (binding.summaryCard.getVisibility() == View.VISIBLE || binding.editorCard.getVisibility() == View.VISIBLE) {
            showHomeScreen();
            return;
        }
        super.onBackPressed();
    }

    @Override
    protected void onStart() {
        super.onStart();
        pollHandler.postDelayed(pollRunnable, POLL_INTERVAL_MS);
    }

    @Override
    protected void onStop() {
        saveSettingsForm();
        pollHandler.removeCallbacks(pollRunnable);
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        repository.shutdown();
        super.onDestroy();
    }

    private void configureUi() {
        binding.gatewayUrlInput.setText(preferences.loadBaseUrl());
        binding.wifiSsidInput.setText(preferences.getWifiSsid());
        binding.wifiPasswordInput.setText(preferences.getWifiPassword());
        binding.ipAddressInput.setText(preferences.getWifiIp());
        binding.dhcpSwitch.setChecked(preferences.isWifiDhcp());

        binding.setpointSlider.setValueFrom(MIN_SETPOINT);
        binding.setpointSlider.setValueTo(MAX_SETPOINT);
        binding.setpointSlider.setStepSize(1f);
        binding.setpointSlider.setValue(preferences.loadDemoState().setpoint);
        binding.setpointSlider.addOnChangeListener((slider, value, fromUser) ->
                binding.targetTempValue.setText(formatTemperature(value)));
        binding.setpointSlider.addOnSliderTouchListener(new Slider.OnSliderTouchListener() {
            @Override
            public void onStartTrackingTouch(@NonNull Slider slider) {
                sliderTracking = true;
            }

            @Override
            public void onStopTrackingTouch(@NonNull Slider slider) {
                sliderTracking = false;
            }
        });

        binding.zoneCountSlider.setValueFrom(1f);
        binding.zoneCountSlider.setValueTo(15f);
        binding.zoneCountSlider.setStepSize(1f);
        binding.zoneCountSlider.setValue(preferences.getZoneCount());
        binding.zoneCountSlider.addOnChangeListener((slider, value, fromUser) -> {
            int count = Math.round(value);
            preferences.setZoneCount(count);
            binding.zoneCountValue.setText(String.valueOf(count));
            binding.zoneCountSummaryValue.setText(String.valueOf(count));
            if (editingZone > count) {
                editingZone = count;
            }
            updateZoneSelector();
            renderZoneGrid();
            binding.statusValue.setText(getString(R.string.status_zone_saved));
        });

        binding.menuButton.setOnClickListener(v -> showOverflowMenu());
        binding.refreshButton.setOnClickListener(v -> refreshState(true));
        binding.applyButton.setOnClickListener(v -> applySetpoint());
        binding.pairButton.setOnClickListener(v -> requestPairing());

        setupModeSelection();
        updateZoneSelector();
        bindScheduleButtons();
        bindLanguageSpinner();
        renderModeSelection(preferences.getMode());
        syncZoneSelectionUi();
    }

    private void setupModeSelection() {
        binding.coolingCheck.setOnClickListener(v -> selectMode("cooling", true));
        binding.heatingCheck.setOnClickListener(v -> selectMode("heating", true));
        binding.autoCheck.setOnClickListener(v -> selectMode("auto", true));
        binding.ventilationCheck.setOnClickListener(v -> selectMode("ventilation", true));
    }

    private ArrayAdapter<String> buildSpinnerAdapter(List<String> items) {
        ArrayAdapter<String> adapter = new ArrayAdapter<>(this, R.layout.spinner_item_light, items);
        adapter.setDropDownViewResource(R.layout.spinner_dropdown_item_light);
        return adapter;
    }

    private void bindLanguageSpinner() {
        List<String> languages = new ArrayList<>();
        languages.add(getString(R.string.language_polish));
        languages.add(getString(R.string.language_english));
        languages.add(getString(R.string.language_ukrainian));

        binding.languageSpinner.setAdapter(buildSpinnerAdapter(languages));

        int selection = 0;
        String currentLanguage = preferences.getLanguage();
        for (int i = 0; i < languageCodes.length; i++) {
            if (languageCodes[i].equals(currentLanguage)) {
                selection = i;
                break;
            }
        }
        binding.languageSpinner.setSelection(selection, false);
        languageReady = true;

        binding.languageSpinner.setOnItemSelectedListener(new SimpleItemSelectedListener() {
            @Override
            public void onItemSelected(int position) {
                if (!languageReady) {
                    return;
                }
                String selectedCode = languageCodes[position];
                if (!selectedCode.equals(preferences.getLanguage())) {
                    saveSettingsForm();
                    preferences.setLanguage(selectedCode);
                    binding.statusValue.setText(getString(R.string.status_language_saved));
                    restartForLanguage();
                }
            }
        });
    }

    private void restartForLanguage() {
        Intent intent = getIntent();
        finish();
        startActivity(intent);
        overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out);
    }

    private void bindScheduleButtons() {
        binding.schedule1StartButton.setOnClickListener(v -> showTimePicker(binding.schedule1StartButton, 1, true));
        binding.schedule1EndButton.setOnClickListener(v -> showTimePicker(binding.schedule1EndButton, 1, false));
        binding.schedule2StartButton.setOnClickListener(v -> showTimePicker(binding.schedule2StartButton, 2, true));
        binding.schedule2EndButton.setOnClickListener(v -> showTimePicker(binding.schedule2EndButton, 2, false));
        binding.schedule3StartButton.setOnClickListener(v -> showTimePicker(binding.schedule3StartButton, 3, true));
        binding.schedule3EndButton.setOnClickListener(v -> showTimePicker(binding.schedule3EndButton, 3, false));
    }

    private void showTimePicker(Button button, int slotIndex, boolean start) {
        int[] parsed = parseHourMinute(button.getText().toString());
        TimePickerDialog dialog = new TimePickerDialog(this, (view, hourOfDay, minute) -> {
            String value = String.format(Locale.US, "%02d:%02d", hourOfDay, minute);
            button.setText(value);
            if (start) {
                preferences.setScheduleStart(editingZone, slotIndex, value);
            } else {
                preferences.setScheduleEnd(editingZone, slotIndex, value);
            }
            renderZoneGrid();
            binding.statusValue.setText(getString(R.string.status_zone_saved));
        }, parsed[0], parsed[1], true);
        dialog.show();
    }

    private void updateZoneSelector() {
        List<String> zones = new ArrayList<>();
        for (int i = 1; i <= GatewayPreferences.MAX_ZONES; i++) {
            zones.add(getString(R.string.zone_name, i));
        }

        binding.zoneSelectorSpinner.setAdapter(buildSpinnerAdapter(zones));
        int selection = Math.max(0, Math.min(GatewayPreferences.MAX_ZONES - 1, editingZone - 1));
        binding.zoneSelectorSpinner.setSelection(selection, false);
        editingZone = selection + 1;
        loadScheduleForZone(editingZone);
        binding.zoneSelectorSpinner.setOnItemSelectedListener(new SimpleItemSelectedListener() {
            @Override
            public void onItemSelected(int position) {
                editingZone = position + 1;
                loadScheduleForZone(editingZone);
            }
        });
    }

    private void loadScheduleForZone(int zoneIndex) {
        binding.schedule1StartButton.setText(preferences.getScheduleStart(zoneIndex, 1));
        binding.schedule1EndButton.setText(preferences.getScheduleEnd(zoneIndex, 1));
        binding.schedule2StartButton.setText(preferences.getScheduleStart(zoneIndex, 2));
        binding.schedule2EndButton.setText(preferences.getScheduleEnd(zoneIndex, 2));
        binding.schedule3StartButton.setText(preferences.getScheduleStart(zoneIndex, 3));
        binding.schedule3EndButton.setText(preferences.getScheduleEnd(zoneIndex, 3));
    }

    private void syncZoneSelectionUi() {
        int visibleCount = preferences.getVisibleZones().size();
        binding.zoneCountValue.setText(String.valueOf(visibleCount));
        binding.zoneCountSummaryValue.setText(String.valueOf(visibleCount));
        renderZoneGrid();
    }

    private void showHomeScreen() {
        binding.summaryCard.setVisibility(View.GONE);
        binding.editorCard.setVisibility(View.GONE);
        binding.contentScroll.post(() -> binding.contentScroll.smoothScrollTo(0, 0));
    }

    private void showZoneSelectionDialog() {
        CharSequence[] zoneItems = new CharSequence[GatewayPreferences.MAX_ZONES];
        boolean[] checkedItems = new boolean[GatewayPreferences.MAX_ZONES];

        for (int i = 1; i <= GatewayPreferences.MAX_ZONES; i++) {
            zoneItems[i - 1] = getString(R.string.zone_name, i);
            checkedItems[i - 1] = preferences.isZoneVisible(i);
        }

        new MaterialAlertDialogBuilder(this)
                .setTitle(R.string.main_screen_zones)
                .setMultiChoiceItems(zoneItems, checkedItems, (dialog, which, isChecked) ->
                        preferences.setZoneVisible(which + 1, isChecked))
                .setPositiveButton(android.R.string.ok, (dialog, which) -> {
                    syncZoneSelectionUi();
                    showHomeScreen();
                })
                .setOnDismissListener(dialog -> {
                    syncZoneSelectionUi();
                    showHomeScreen();
                })
                .show();
    }

    private void showZoneControlDialog(int zoneIndex) {
        View dialogView = getLayoutInflater().inflate(R.layout.dialog_zone_panel, null);
        TextView zoneTitle = dialogView.findViewById(R.id.zonePanelName);
        TextView currentTempValue = dialogView.findViewById(R.id.zonePanelCurrentTemp);
        TextView setpointValue = dialogView.findViewById(R.id.zonePanelSetpointValue);
        Slider setpointSlider = dialogView.findViewById(R.id.zonePanelSlider);
        View decreaseButton = dialogView.findViewById(R.id.zonePanelDecreaseButton);
        View increaseButton = dialogView.findViewById(R.id.zonePanelIncreaseButton);
        View closeButton = dialogView.findViewById(R.id.zonePanelCloseButton);
        View chronogramButton = dialogView.findViewById(R.id.zonePanelChronogramButton);
        View confirmButton = dialogView.findViewById(R.id.zonePanelConfirmButton);

        float zoneTemperature = getZoneTemperature(zoneIndex);
        float zoneSetpoint = preferences.getZoneSetpoint(zoneIndex, currentState == null ? 22f : currentState.setpoint);

        zoneTitle.setText(getString(R.string.zone_name, zoneIndex));
        currentTempValue.setText(formatTemperature(zoneTemperature));
        setpointSlider.setValueFrom(MIN_SETPOINT);
        setpointSlider.setValueTo(MAX_SETPOINT);
        setpointSlider.setStepSize(0.5f);
        setpointSlider.setValue(zoneSetpoint);
        setpointValue.setText(getString(R.string.zone_panel_threshold, formatTemperature(zoneSetpoint)));
        setpointSlider.addOnChangeListener((slider, value, fromUser) ->
                setpointValue.setText(getString(R.string.zone_panel_threshold, formatTemperature(value))));

        AlertDialog dialog = new MaterialAlertDialogBuilder(this)
                .setView(dialogView)
                .create();

        decreaseButton.setOnClickListener(v -> {
            float updatedValue = Math.max(MIN_SETPOINT, setpointSlider.getValue() - 0.5f);
            setpointSlider.setValue(updatedValue);
        });
        increaseButton.setOnClickListener(v -> {
            float updatedValue = Math.min(MAX_SETPOINT, setpointSlider.getValue() + 0.5f);
            setpointSlider.setValue(updatedValue);
        });
        closeButton.setOnClickListener(v -> dialog.dismiss());
        chronogramButton.setOnClickListener(v -> {
            dialog.dismiss();
            openZoneChronogram(zoneIndex);
        });
        confirmButton.setOnClickListener(v -> {
            preferences.setZoneSetpoint(zoneIndex, setpointSlider.getValue());
            renderZoneGrid();
            dialog.dismiss();
        });

        dialog.show();
    }

    private void openZoneChronogram(int zoneIndex) {
        editingZone = zoneIndex;
        updateZoneSelector();
        loadScheduleForZone(zoneIndex);
        binding.zoneScheduleName.setText(getString(R.string.zone_name, zoneIndex));
        showScreen(MenuScreen.ZONES);
    }

    private void showOverflowMenu() {
        PopupMenu popupMenu = new PopupMenu(
                new ContextThemeWrapper(this, R.style.ThemeOverlayTermostatAndroidPopupMenu),
                binding.menuButton
        );
        popupMenu.getMenuInflater().inflate(R.menu.main_menu, popupMenu.getMenu());
        popupMenu.setOnMenuItemClickListener(this::onMenuSelected);
        popupMenu.show();
    }

    private boolean onMenuSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_zones) {
            showZoneSelectionDialog();
            return true;
        }
        if (item.getItemId() == R.id.menu_modes) {
            showScreen(MenuScreen.MODES);
            return true;
        }
        if (item.getItemId() == R.id.menu_settings) {
            showScreen(MenuScreen.SETTINGS);
            return true;
        }
        return false;
    }

    private void showScreen(MenuScreen screen) {
        showScreen(screen, true);
    }

    private void showScreen(MenuScreen screen, boolean scrollToEditor) {
        binding.summaryCard.setVisibility(screen == MenuScreen.MODES ? View.VISIBLE : View.GONE);
        binding.editorCard.setVisibility(View.VISIBLE);
        binding.zonesSection.setVisibility(screen == MenuScreen.ZONES ? View.VISIBLE : View.GONE);
        binding.modeSection.setVisibility(screen == MenuScreen.MODES ? View.VISIBLE : View.GONE);
        binding.settingsSection.setVisibility(screen == MenuScreen.SETTINGS ? View.VISIBLE : View.GONE);

        if (screen == MenuScreen.ZONES) {
            binding.editorTitle.setText(getString(R.string.zone_schedule_title, getString(R.string.zone_name, editingZone)));
            binding.editorSubtitle.setText(R.string.zone_schedule_subtitle);
            binding.zoneSelectionControls.setVisibility(View.GONE);
            binding.zoneScheduleName.setText(getString(R.string.zone_name, editingZone));
        } else if (screen == MenuScreen.MODES) {
            binding.editorTitle.setText(R.string.menu_modes);
            binding.editorSubtitle.setText(R.string.modes_subtitle);
        } else {
            binding.editorTitle.setText(R.string.menu_settings);
            binding.editorSubtitle.setText(R.string.settings_subtitle);
        }

        if (scrollToEditor) {
            binding.contentScroll.post(() -> binding.contentScroll.smoothScrollTo(0, binding.editorCard.getTop()));
        }
    }

    private void selectMode(String mode, boolean announce) {
        preferences.setMode(mode);
        renderModeSelection(mode);
        renderState(currentState == null ? preferences.loadDemoState() : currentState);
        if (announce) {
            binding.statusValue.setText(getString(R.string.mode_selected, getModeLabel(mode)));
        }
    }

    private void renderModeSelection(String mode) {
        binding.coolingCheck.setChecked("cooling".equals(mode));
        binding.heatingCheck.setChecked("heating".equals(mode));
        binding.autoCheck.setChecked("auto".equals(mode));
        binding.ventilationCheck.setChecked("ventilation".equals(mode));
        binding.headerModeValue.setText(getModeLabel(mode));
    }

    private void refreshState(boolean explicitRefresh) {
        saveSettingsForm();
        if (explicitRefresh) {
            binding.statusValue.setText(getString(R.string.status_syncing));
        }

        repository.fetchState(preferences.loadBaseUrl(), preferences.isDemoMode(), new ThermostatRepository.Callback() {
            @Override
            public void onSuccess(ThermostatState state) {
                renderState(state);
            }

            @Override
            public void onError(String message) {
                binding.statusValue.setText(message);
            }
        });
    }

    private void applySetpoint() {
        saveSettingsForm();
        int setpoint = Math.round(binding.setpointSlider.getValue());
        binding.statusValue.setText(getString(R.string.status_applying));
        repository.updateSetpoint(preferences.loadBaseUrl(), preferences.isDemoMode(), setpoint, new ThermostatRepository.Callback() {
            @Override
            public void onSuccess(ThermostatState state) {
                renderState(state);
            }

            @Override
            public void onError(String message) {
                binding.statusValue.setText(message);
            }
        });
    }

    private void requestPairing() {
        saveSettingsForm();
        binding.statusValue.setText(getString(R.string.status_pairing));
        repository.requestPair(preferences.loadBaseUrl(), preferences.isDemoMode(), new ThermostatRepository.Callback() {
            @Override
            public void onSuccess(ThermostatState state) {
                renderState(state);
            }

            @Override
            public void onError(String message) {
                binding.statusValue.setText(message);
            }
        });
    }

    private void renderState(ThermostatState state) {
        currentState = state;
        binding.currentTempValue.setText(formatTemperature(state.currentTemperature));
        binding.targetTempValue.setText(formatTemperature(state.setpoint));
        if (!sliderTracking) {
            binding.setpointSlider.setValue(state.setpoint);
        }

        ActionState actionState = evaluateAction(state.currentTemperature, state.setpoint);
        binding.sourceBadge.setText(preferences.isDemoMode() ? R.string.source_demo : R.string.source_live);
        binding.pairStatusValue.setText(state.paired ? R.string.pair_connected : R.string.pair_waiting);
        binding.lastUpdatedValue.setText(getString(R.string.last_updated, formatTime(state.updatedAtMillis)));
        binding.systemStatusValue.setText(getStatusLabel(actionState));
        binding.zoneCountSummaryValue.setText(String.valueOf(preferences.getVisibleZones().size()));

        String statusMessage = state.statusMessage == null ? "" : state.statusMessage.trim();
        if (statusMessage.isEmpty()) {
            binding.statusValue.setText(getStatusLabel(actionState));
        } else {
            binding.statusValue.setText(statusMessage);
        }

        updateAccentForAction(actionState);
        renderZoneGrid();
    }

    private void renderZoneGrid() {
        binding.zoneGridContainer.removeAllViews();
        for (int zoneIndex : preferences.getVisibleZones()) {
            View zoneCard = createZoneCard(zoneIndex);
            LinearLayout.LayoutParams cardParams = new LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
            );
            if (binding.zoneGridContainer.getChildCount() > 0) {
                cardParams.topMargin = dp(10);
            }
            zoneCard.setLayoutParams(cardParams);
            binding.zoneGridContainer.addView(zoneCard);
        }
    }

    private View createZoneCard(int zoneIndex) {
        MaterialCardView cardView = new MaterialCardView(this);
        cardView.setRadius(dp(20));
        cardView.setCardBackgroundColor(ContextCompat.getColor(this, android.R.color.white));
        cardView.setCardElevation(dp(2));
        cardView.setStrokeWidth(0);
        cardView.setOnClickListener(v -> showZoneControlDialog(zoneIndex));

        float zoneTemperature = getZoneTemperature(zoneIndex);
        float zoneSetpoint = preferences.getZoneSetpoint(zoneIndex, currentState == null ? 22f : currentState.setpoint);
        ActionState zoneAction = evaluateAction(zoneTemperature, zoneSetpoint);

        LinearLayout shell = new LinearLayout(this);
        shell.setOrientation(LinearLayout.HORIZONTAL);
        shell.setMinimumHeight(dp(96));
        cardView.addView(shell);

        LinearLayout infoRow = new LinearLayout(this);
        infoRow.setOrientation(LinearLayout.HORIZONTAL);
        infoRow.setGravity(Gravity.CENTER_VERTICAL);
        infoRow.setPadding(dp(14), dp(12), dp(12), dp(12));
        infoRow.setLayoutParams(new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f));
        shell.addView(infoRow);

        LinearLayout iconButton = new LinearLayout(this);
        iconButton.setGravity(Gravity.CENTER);
        iconButton.setBackground(ContextCompat.getDrawable(this, R.drawable.bg_zone_dialog_icon));
        LinearLayout.LayoutParams iconParams = new LinearLayout.LayoutParams(dp(44), dp(44));
        iconButton.setLayoutParams(iconParams);
        iconButton.setOnClickListener(v -> showZoneControlDialog(zoneIndex));

        ImageView iconGlyph = new ImageView(this);
        iconGlyph.setImageResource(R.drawable.ic_ajax_peak);
        iconGlyph.setColorFilter(ContextCompat.getColor(this, android.R.color.white));
        LinearLayout.LayoutParams glyphParams = new LinearLayout.LayoutParams(dp(20), dp(20));
        iconGlyph.setLayoutParams(glyphParams);
        iconButton.addView(iconGlyph);
        infoRow.addView(iconButton);

        LinearLayout content = new LinearLayout(this);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setGravity(Gravity.CENTER_HORIZONTAL);
        LinearLayout.LayoutParams contentParams = new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f);
        contentParams.setMarginStart(dp(12));
        content.setLayoutParams(contentParams);
        infoRow.addView(content);

        TextView title = createCardText(getString(R.string.zone_name, zoneIndex), 16, true, R.color.panel_stroke, Gravity.CENTER_HORIZONTAL);
        TextView temperature = createCardText(formatTemperature(zoneTemperature), 34, true, R.color.storm_950, Gravity.CENTER_HORIZONTAL);
        TextView stateView = createCardText(getZoneStateLabel(zoneAction), 13, false, R.color.panel_stroke, Gravity.CENTER_HORIZONTAL);

        content.addView(title);
        content.addView(temperature);
        content.addView(stateView);

        LinearLayout setpointPanel = new LinearLayout(this);
        setpointPanel.setOrientation(LinearLayout.VERTICAL);
        setpointPanel.setGravity(Gravity.CENTER);
        setpointPanel.setBackground(ContextCompat.getDrawable(this, R.drawable.bg_zone_setpoint));
        setpointPanel.setPadding(dp(8), dp(12), dp(8), dp(12));
        setpointPanel.setLayoutParams(new LinearLayout.LayoutParams(dp(92), LinearLayout.LayoutParams.MATCH_PARENT));

        TextView setpoint = createCardText(
                formatTemperature(zoneSetpoint),
                24,
                true,
                android.R.color.white,
                Gravity.CENTER
        );
        setpointPanel.addView(setpoint);
        shell.addView(setpointPanel);

        return cardView;
    }

    private TextView createCardText(String text, int sp, boolean bold, int colorResId, int gravity) {
        TextView view = new TextView(this);
        view.setText(text);
        view.setTextColor(ContextCompat.getColor(this, colorResId));
        view.setTextSize(sp);
        view.setGravity(gravity);
        if (bold) {
            view.setTypeface(view.getTypeface(), android.graphics.Typeface.BOLD);
        }
        return view;
    }

    private ActionState evaluateAction(float currentTemperature, float setpoint) {
        String mode = preferences.getMode();
        if ("cooling".equals(mode)) {
            return currentTemperature > setpoint ? ActionState.COOL : ActionState.IDLE;
        }
        if ("heating".equals(mode)) {
            return currentTemperature < setpoint ? ActionState.HEAT : ActionState.IDLE;
        }
        if ("ventilation".equals(mode)) {
            return ActionState.FAN;
        }
        if (currentTemperature <= setpoint - 1) {
            return ActionState.HEAT;
        }
        if (currentTemperature >= setpoint + 1) {
            return ActionState.COOL;
        }
        return ActionState.IDLE;
    }

    private void updateAccentForAction(ActionState actionState) {
        ViewCompat.setBackgroundTintList(binding.sourceBadge, ColorStateList.valueOf(getActionColor(actionState)));
    }

    private int getActionColor(ActionState actionState) {
        if (actionState == ActionState.HEAT) {
            return ContextCompat.getColor(this, R.color.brand_orange_400);
        }
        if (actionState == ActionState.COOL) {
            return ContextCompat.getColor(this, R.color.brand_teal_400);
        }
        if (actionState == ActionState.FAN) {
            return ContextCompat.getColor(this, R.color.success_green);
        }
        return ContextCompat.getColor(this, R.color.frost_100);
    }

    private String getStatusLabel(ActionState actionState) {
        if (actionState == ActionState.HEAT) {
            return getString(R.string.status_heating_active);
        }
        if (actionState == ActionState.COOL) {
            return getString(R.string.status_cooling_active);
        }
        if (actionState == ActionState.FAN) {
            return getString(R.string.status_fan_active);
        }
        if ("auto".equals(preferences.getMode())) {
            return getString(R.string.status_auto_idle);
        }
        return getString(R.string.status_idle);
    }

    private String getZoneStateLabel(ActionState actionState) {
        if (actionState == ActionState.HEAT) {
            return getString(R.string.zone_card_status_heat);
        }
        if (actionState == ActionState.COOL) {
            return getString(R.string.zone_card_status_cool);
        }
        if (actionState == ActionState.FAN) {
            return getString(R.string.zone_card_status_fan);
        }
        return getString(R.string.zone_card_status_idle);
    }

    private String getModeLabel(String mode) {
        if ("cooling".equals(mode)) {
            return getString(R.string.mode_cooling);
        }
        if ("heating".equals(mode)) {
            return getString(R.string.mode_heating);
        }
        if ("ventilation".equals(mode)) {
            return getString(R.string.mode_ventilation);
        }
        return getString(R.string.mode_auto);
    }

    private float getZoneTemperature(int zoneIndex) {
        float base = currentState == null ? 22f : currentState.currentTemperature;
        float[] deltas = new float[] {-1.1f, -0.4f, 0.3f, 0.8f, 1.4f};
        float value = base + deltas[(zoneIndex - 1) % deltas.length];
        return Math.max(5f, Math.min(35f, value));
    }

    private void saveSettingsForm() {
        CharSequence gateway = binding.gatewayUrlInput.getText();
        CharSequence ssid = binding.wifiSsidInput.getText();
        CharSequence password = binding.wifiPasswordInput.getText();
        CharSequence ip = binding.ipAddressInput.getText();

        preferences.saveBaseUrl(gateway == null ? "" : gateway.toString());
        preferences.setWifiSsid(ssid == null ? "" : ssid.toString());
        preferences.setWifiPassword(password == null ? "" : password.toString());
        preferences.setWifiDhcp(binding.dhcpSwitch.isChecked());
        preferences.setWifiIp(ip == null ? "" : ip.toString());
    }

    private int[] parseHourMinute(String value) {
        try {
            String[] parts = value.split(":");
            return new int[] {Integer.parseInt(parts[0]), Integer.parseInt(parts[1])};
        } catch (Exception ignored) {
            return new int[] {0, 0};
        }
    }

    private int dp(int value) {
        float density = getResources().getDisplayMetrics().density;
        return Math.round(value * density);
    }

    private String formatTemperature(float value) {
        return String.format(Locale.US, "%.1f\u00B0C", value);
    }

    private String formatTime(long timeMillis) {
        return DateFormat.getTimeInstance(DateFormat.SHORT).format(new Date(timeMillis));
    }

    private abstract static class SimpleItemSelectedListener implements android.widget.AdapterView.OnItemSelectedListener {
        @Override
        public void onItemSelected(android.widget.AdapterView<?> parent, View view, int position, long id) {
            onItemSelected(position);
        }

        @Override
        public void onNothingSelected(android.widget.AdapterView<?> parent) {
        }

        public abstract void onItemSelected(int position);
    }
}
