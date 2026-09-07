package com.example.androi.ui.components

import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.PowerSettingsNew
import androidx.compose.material3.Icon
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.example.androi.ui.theme.BrandOrange
import com.example.androi.ui.theme.SurfaceVariant
import com.example.androi.ui.theme.TextMuted

/**
 * Nút Switch con nhộng đặc trưng của Template:
 * - Khi Bật: Màu cam BrandOrange, nút tròn trắng bên phải có icon Power
 * - Khi Tắt: Màu xám SurfaceVariant, nút tròn trắng bên trái có icon Power
 */
@Composable
fun SmartSwitch(
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
    modifier: Modifier = Modifier,
    width: Dp = 54.dp,
    height: Dp = 30.dp
) {
    val backgroundColor by animateColorAsState(
        targetValue = if (checked) BrandOrange else SurfaceVariant,
        label = "switchBg"
    )

    val thumbOffset by animateDpAsState(
        targetValue = if (checked) (width - height) else 0.dp,
        label = "thumbOffset"
    )

    val interactionSource = remember { MutableInteractionSource() }

    Box(
        modifier = modifier
            .width(width)
            .height(height)
            .clip(RoundedCornerShape(height / 2))
            .background(backgroundColor)
            .clickable(
                interactionSource = interactionSource,
                indication = null
            ) {
                onCheckedChange(!checked)
            }
            .padding(2.dp),
        contentAlignment = Alignment.CenterStart
    ) {
        Box(
            modifier = Modifier
                .offset(x = thumbOffset)
                .size(height - 4.dp)
                .clip(CircleShape)
                .background(Color.White),
            contentAlignment = Alignment.Center
        ) {
            Icon(
                imageVector = Icons.Rounded.PowerSettingsNew,
                contentDescription = "Power",
                tint = if (checked) BrandOrange else TextMuted,
                modifier = Modifier.size(14.dp)
            )
        }
    }
}
