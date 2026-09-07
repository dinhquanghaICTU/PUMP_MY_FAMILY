package com.example.androi.ui.components

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.*
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.example.androi.ui.theme.BorderLight
import com.example.androi.ui.theme.BrandOrange
import com.example.androi.ui.theme.TextPrimary
import com.example.androi.ui.theme.TextSecondary
import kotlin.math.cos
import kotlin.math.sin

/**
 * Đồng hồ đo hình cánh cung (Semicircular Arc Gauge) chuẩn theo Screen 3 trong template
 * Hiển thị phần trăm nước (0% -> 100%) kèm kim chỉ vị trí hình tròn.
 */
@Composable
fun CircularArcGauge(
    value: Float, // 0f .. 100f
    modifier: Modifier = Modifier,
    unit: String = "%",
    label: String = "Mực Nước Téc",
    strokeWidth: Float = 28f
) {
    val clampedValue = value.coerceIn(0f, 100f)
    val animatedValue by animateFloatAsState(
        targetValue = clampedValue,
        animationSpec = tween(durationMillis = 800),
        label = "arcGaugeAnim"
    )

    Box(
        modifier = modifier
            .fillMaxWidth()
            .height(210.dp),
        contentAlignment = Alignment.Center
    ) {
        Canvas(
            modifier = Modifier
                .fillMaxWidth()
                .height(180.dp)
                .padding(horizontal = 24.dp)
        ) {
            val startAngle = 180f
            val sweepAngle = 180f
            val arcSweep = (animatedValue / 100f) * sweepAngle

            val diameter = size.width - strokeWidth
            val arcSize = Size(diameter, diameter)
            val topLeft = Offset(strokeWidth / 2, strokeWidth / 2)

            // 1. Vẽ vòng cung xám nền
            drawArc(
                color = BorderLight,
                startAngle = startAngle,
                sweepAngle = sweepAngle,
                useCenter = false,
                topLeft = topLeft,
                size = arcSize,
                style = Stroke(width = strokeWidth, cap = StrokeCap.Round)
            )

            // 2. Vẽ vòng cung màu cam tiến độ
            if (arcSweep > 0) {
                drawArc(
                    color = BrandOrange,
                    startAngle = startAngle,
                    sweepAngle = arcSweep,
                    useCenter = false,
                    topLeft = topLeft,
                    size = arcSize,
                    style = Stroke(width = strokeWidth, cap = StrokeCap.Round)
                )
            }

            // 3. Vẽ nút chấm tròn (Thumb) ở điểm cuối tiến độ
            val currentAngleRad = Math.toRadians((startAngle + arcSweep).toDouble())
            val radius = diameter / 2
            val centerX = topLeft.x + radius
            val centerY = topLeft.y + radius

            val thumbX = centerX + radius * cos(currentAngleRad).toFloat()
            val thumbY = centerY + radius * sin(currentAngleRad).toFloat()

            // Viền ngoài của Thumb
            drawCircle(
                color = BrandOrange,
                radius = strokeWidth * 0.8f,
                center = Offset(thumbX, thumbY)
            )
            // Lõi trắng bên trong Thumb
            drawCircle(
                color = Color.White,
                radius = strokeWidth * 0.4f,
                center = Offset(thumbX, thumbY)
            )
        }

        // Chữ ở trung tâm: 89% và "Mực Nước Téc"
        Column(
            horizontalAlignment = Alignment.CenterHorizontally,
            modifier = Modifier.padding(top = 40.dp)
        ) {
            Text(
                text = "${animatedValue.toInt()}$unit",
                fontSize = 44.sp,
                fontWeight = FontWeight.Bold,
                color = TextPrimary
            )
            Spacer(modifier = Modifier.height(4.dp))
            Text(
                text = label,
                fontSize = 14.sp,
                color = TextSecondary
            )
        }

        // Nhãn 0% (trái) và 100% (phải) ở đáy vòng cung
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 40.dp)
                .align(Alignment.BottomCenter),
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Text(
                text = "0$unit",
                fontSize = 12.sp,
                fontWeight = FontWeight.Medium,
                color = TextSecondary
            )
            Text(
                text = "100$unit",
                fontSize = 12.sp,
                fontWeight = FontWeight.Medium,
                color = TextSecondary
            )
        }
    }
}
