package com.termostat.android;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.util.AttributeSet;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;

public class TemperatureHistoryView extends View {
    private final Paint gridPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint linePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint pointPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path path = new Path();
    private float[] temperatures = new float[] {20f, 21f, 21.5f, 22f, 21.8f, 22.2f, 23f, 22.6f, 22.8f, 23.4f, 22.9f, 22.1f};

    public TemperatureHistoryView(Context context) {
        super(context);
        init();
    }

    public TemperatureHistoryView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public TemperatureHistoryView(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    private void init() {
        gridPaint.setColor(ContextCompat.getColor(getContext(), R.color.panel_stroke));
        gridPaint.setStrokeWidth(2f);

        linePaint.setColor(ContextCompat.getColor(getContext(), R.color.brand_orange_300));
        linePaint.setStrokeWidth(6f);
        linePaint.setStyle(Paint.Style.STROKE);
        linePaint.setStrokeCap(Paint.Cap.ROUND);
        linePaint.setStrokeJoin(Paint.Join.ROUND);

        pointPaint.setColor(ContextCompat.getColor(getContext(), R.color.brand_teal_300));
        pointPaint.setStyle(Paint.Style.FILL);
    }

    public void setTemperatures(float[] values) {
        if (values == null || values.length < 2) {
            return;
        }
        temperatures = values.clone();
        invalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        float width = getWidth();
        float height = getHeight();
        float padding = 28f;
        float chartWidth = width - padding * 2f;
        float chartHeight = height - padding * 2f;

        for (int i = 0; i < 4; i++) {
            float y = padding + (chartHeight / 3f) * i;
            canvas.drawLine(padding, y, width - padding, y, gridPaint);
        }

        float min = temperatures[0];
        float max = temperatures[0];
        for (float value : temperatures) {
            min = Math.min(min, value);
            max = Math.max(max, value);
        }
        if (max - min < 1f) {
            max += 0.5f;
            min -= 0.5f;
        }

        path.reset();
        for (int i = 0; i < temperatures.length; i++) {
            float x = padding + chartWidth * i / (temperatures.length - 1f);
            float normalized = (temperatures[i] - min) / (max - min);
            float y = padding + chartHeight - (normalized * chartHeight);
            if (i == 0) {
                path.moveTo(x, y);
            } else {
                path.lineTo(x, y);
            }
        }

        canvas.drawPath(path, linePaint);

        for (int i = 0; i < temperatures.length; i++) {
            float x = padding + chartWidth * i / (temperatures.length - 1f);
            float normalized = (temperatures[i] - min) / (max - min);
            float y = padding + chartHeight - (normalized * chartHeight);
            canvas.drawCircle(x, y, 7f, pointPaint);
        }
    }
}
