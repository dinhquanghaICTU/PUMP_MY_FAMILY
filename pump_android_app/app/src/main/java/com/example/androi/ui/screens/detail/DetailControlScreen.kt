package com.example.androi.ui.screens.detail

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.ArrowBack
import androidx.compose.material.icons.rounded.*
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.example.androi.data.model.DeviceUiState
import com.example.androi.ui.components.CircularArcGauge
import com.example.androi.ui.components.SmartSwitch
import com.example.androi.ui.theme.*

/**
 * Màn hình Chi Tiết Thiết Bị (Detail Screen):
 * - Hiển thị Trạng thái bể nước (Đồng hồ cánh cung Arc Gauge, % nước, khoảng cách cm, pin ESP-NOW)
 * - Nút điều khiển máy bơm & chế độ hoạt động
 * - Tính năng KHÓA TRẺ EM (Child Lock) bảo vệ an toàn nút bấm vật lý trên tủ ESP32
 * - Cài đặt cấu hình ngưỡng tự động (Auto Thresholds)
 */
@Composable
fun DetailControlScreen(
    deviceState: DeviceUiState = DeviceUiState(),
    onTogglePump: (Boolean) -> Unit = {},
    onToggleChildLock: (Boolean) -> Unit = {},
    onToggleAutoMode: (Boolean) -> Unit = {},
    onBackClick: () -> Unit = {},
    modifier: Modifier = Modifier
) {
    var selectedMode by remember { mutableStateOf(if (deviceState.isAutoMode) 0 else 1) }
    val modes = listOf("Tự Động", "Thủ Công", "Hẹn Giờ")

    Column(
        modifier = modifier
            .fillMaxSize()
            .background(ScreenBackground)
            .verticalScroll(rememberScrollState())
            .padding(bottom = 120.dp)
    ) {
        // 1. Top Bar: Nút Back + Tiêu đề + Trạng thái Online
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 24.dp, vertical = 16.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Box(
                modifier = Modifier
                    .size(44.dp)
                    .clip(CircleShape)
                    .background(CardBackground)
                    .clickable { onBackClick() },
                contentAlignment = Alignment.Center
            ) {
                Icon(
                    imageVector = Icons.AutoMirrored.Rounded.ArrowBack,
                    contentDescription = "Back",
                    tint = TextPrimary,
                    modifier = Modifier.size(20.dp)
                )
            }

            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Text(
                    text = "Chi Tiết Máy Bơm",
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold,
                    color = TextPrimary
                )
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Box(
                        modifier = Modifier
                            .size(7.dp)
                            .clip(CircleShape)
                            .background(Color(0xFF10B981))
                    )
                    Spacer(modifier = Modifier.width(5.dp))
                    Text(
                        text = "ESP32-S3 Trực Tuyến",
                        fontSize = 12.sp,
                        color = TextSecondary
                    )
                }
            }

            Box(
                modifier = Modifier
                    .size(44.dp)
                    .clip(CircleShape)
                    .background(CardBackground)
                    .clickable { /* TODO: Cài đặt nâng cao */ },
                contentAlignment = Alignment.Center
            ) {
                Icon(
                    imageVector = Icons.Rounded.Tune,
                    contentDescription = "Config",
                    tint = TextPrimary,
                    modifier = Modifier.size(20.dp)
                )
            }
        }

        Spacer(modifier = Modifier.height(8.dp))

        // 2. Chế độ vận hành (Modes): Tự Động | Thủ Công | Hẹn Giờ
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 24.dp),
            horizontalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            modes.forEachIndexed { index, modeTitle ->
                val isSelected = selectedMode == index
                Box(
                    modifier = Modifier
                        .weight(1f)
                        .height(44.dp)
                        .clip(RoundedCornerShape(22.dp))
                        .background(if (isSelected) BrandOrange else CardBackground)
                        .clickable {
                            selectedMode = index
                            onToggleAutoMode(index == 0)
                        },
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        text = modeTitle,
                        fontSize = 14.sp,
                        fontWeight = if (isSelected) FontWeight.SemiBold else FontWeight.Medium,
                        color = if (isSelected) Color.White else TextPrimary
                    )
                }
            }
        }

        Spacer(modifier = Modifier.height(18.dp))

        // 3. CARD TRẠNG THÁI BỂ NƯỚC (Gauge Arc + Thông số cảm biến ESP-NOW)
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 24.dp)
                .clip(RoundedCornerShape(32.dp))
                .background(CardBackground)
                .padding(20.dp)
        ) {
            Column {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column {
                        Text(
                            text = "Trạng Thái Bể Nước",
                            fontSize = 18.sp,
                            fontWeight = FontWeight.Bold,
                            color = TextPrimary
                        )
                        Text(
                            text = "Cảm biến ESP-NOW Tầng Thượng",
                            fontSize = 13.sp,
                            color = TextSecondary
                        )
                    }

                    Box(
                        modifier = Modifier
                            .clip(RoundedCornerShape(12.dp))
                            .background(SurfaceVariant)
                            .padding(horizontal = 10.dp, vertical = 6.dp)
                    ) {
                        Text(
                            text = "Pin: ${deviceState.batteryVoltage}V",
                            fontSize = 12.sp,
                            fontWeight = FontWeight.SemiBold,
                            color = BrandOrange
                        )
                    }
                }

                Spacer(modifier = Modifier.height(8.dp))

                // Đồng hồ bán nguyệt hiển thị % nước
                CircularArcGauge(
                    value = deviceState.waterPercent,
                    unit = "%",
                    label = "Mực Nước Trong Bể"
                )

                Spacer(modifier = Modifier.height(16.dp))

                // Thống kê nhanh: Khoảng cách mặt nước & Thể tích ước tính
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    Box(
                        modifier = Modifier
                            .weight(1f)
                            .clip(RoundedCornerShape(18.dp))
                            .background(SurfaceVariant)
                            .padding(14.dp)
                    ) {
                        Column {
                            Text(text = "Khoảng cách nước", fontSize = 12.sp, color = TextSecondary)
                            Spacer(modifier = Modifier.height(4.dp))
                            Text(
                                text = "${deviceState.distanceCm} cm",
                                fontSize = 16.sp,
                                fontWeight = FontWeight.Bold,
                                color = TextPrimary
                            )
                        }
                    }

                    Box(
                        modifier = Modifier
                            .weight(1f)
                            .clip(RoundedCornerShape(18.dp))
                            .background(SurfaceVariant)
                            .padding(14.dp)
                    ) {
                        Column {
                            Text(text = "Dung tích chứa", fontSize = 12.sp, color = TextSecondary)
                            Spacer(modifier = Modifier.height(4.dp))
                            Text(
                                text = "~${(deviceState.waterPercent * 20).toInt()} Lít",
                                fontSize = 16.sp,
                                fontWeight = FontWeight.Bold,
                                color = TextPrimary
                            )
                        }
                    }
                }
            }
        }

        Spacer(modifier = Modifier.height(18.dp))

        // 4. CARD ĐIỀU KHIỂN BƠM (Nút Bật / Tắt Bơm nhanh)
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 24.dp)
                .clip(RoundedCornerShape(28.dp))
                .background(CardBackground)
                .padding(20.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Box(
                        modifier = Modifier
                            .size(50.dp)
                            .clip(CircleShape)
                            .background(if (deviceState.isPumpRunning) BrandOrange else SurfaceVariant)
                            .clickable { onTogglePump(!deviceState.isPumpRunning) },
                        contentAlignment = Alignment.Center
                    ) {
                        Icon(
                            imageVector = Icons.Rounded.PowerSettingsNew,
                            contentDescription = "Power",
                            tint = if (deviceState.isPumpRunning) Color.White else TextSecondary,
                            modifier = Modifier.size(26.dp)
                        )
                    }

                    Spacer(modifier = Modifier.width(14.dp))

                    Column {
                        Text(
                            text = "Động Cơ Máy Bơm",
                            fontSize = 16.sp,
                            fontWeight = FontWeight.Bold,
                            color = TextPrimary
                        )
                        Text(
                            text = if (deviceState.isPumpRunning) "Đang hoạt động (Bơm nước)" else "Đang dừng • Sẵn sàng",
                            fontSize = 13.sp,
                            color = if (deviceState.isPumpRunning) BrandOrange else TextSecondary
                        )
                    }
                }

                SmartSwitch(
                    checked = deviceState.isPumpRunning,
                    onCheckedChange = onTogglePump
                )
            }
        }

        Spacer(modifier = Modifier.height(18.dp))

        // 5. TÍNH NĂNG: KHÓA TRẺ EM (CHILD LOCK)
        // Vô hiệu hóa nút bấm vật lý trên tủ điện ESP32 để chống trẻ em nghịch
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 24.dp)
                .clip(RoundedCornerShape(28.dp))
                .background(CardBackground)
                .padding(20.dp)
        ) {
            Column {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        modifier = Modifier.weight(1f)
                    ) {
                        Box(
                            modifier = Modifier
                                .size(46.dp)
                                .clip(CircleShape)
                                .background(if (deviceState.isChildLock) Color(0xFFEF4444) else SurfaceVariant),
                            contentAlignment = Alignment.Center
                        ) {
                            Icon(
                                imageVector = if (deviceState.isChildLock) Icons.Rounded.Lock else Icons.Rounded.LockOpen,
                                contentDescription = "Child Lock",
                                tint = if (deviceState.isChildLock) Color.White else TextSecondary,
                                modifier = Modifier.size(22.dp)
                            )
                        }

                        Spacer(modifier = Modifier.width(14.dp))

                        Column {
                            Text(
                                text = "Khóa Trẻ Em (Child Lock)",
                                fontSize = 16.sp,
                                fontWeight = FontWeight.Bold,
                                color = TextPrimary
                            )
                            Spacer(modifier = Modifier.height(2.dp))
                            Text(
                                text = "Khóa nút bấm cơ tại tủ điện ngoài sân",
                                fontSize = 12.sp,
                                color = TextSecondary
                            )
                        }
                    }

                    SmartSwitch(
                        checked = deviceState.isChildLock,
                        onCheckedChange = onToggleChildLock
                    )
                }

                Spacer(modifier = Modifier.height(14.dp))

                // Cảnh báo trạng thái Khóa Trẻ Em
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(16.dp))
                        .background(if (deviceState.isChildLock) Color(0xFFFEF2F2) else SurfaceVariant)
                        .padding(horizontal = 14.dp, vertical = 10.dp)
                ) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Icon(
                            imageVector = if (deviceState.isChildLock) Icons.Rounded.Shield else Icons.Rounded.Info,
                            contentDescription = null,
                            tint = if (deviceState.isChildLock) Color(0xFFEF4444) else TextSecondary,
                            modifier = Modifier.size(18.dp)
                        )
                        Spacer(modifier = Modifier.width(8.dp))
                        Text(
                            text = if (deviceState.isChildLock)
                                "ĐANG KHÓA AN TOÀN: Nút cứng tại tủ ESP32 bị vô hiệu hóa."
                            else
                                "ĐANG MỞ: Có thể ấn nút trực tiếp trên tủ để bật máy bơm.",
                            fontSize = 12.sp,
                            color = if (deviceState.isChildLock) Color(0xFFDC2626) else TextSecondary,
                            fontWeight = if (deviceState.isChildLock) FontWeight.Medium else FontWeight.Normal
                        )
                    }
                }
            }
        }

        Spacer(modifier = Modifier.height(18.dp))

        // 6. CÀI ĐẶT NGƯỠNG TỰ ĐỘNG (Auto Threshold Settings)
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 24.dp)
                .clip(RoundedCornerShape(28.dp))
                .background(CardBackground)
                .padding(20.dp)
        ) {
            Column {
                Text(
                    text = "Cài Đặt Ngưỡng Tự Động",
                    fontSize = 17.sp,
                    fontWeight = FontWeight.Bold,
                    color = TextPrimary
                )
                Spacer(modifier = Modifier.height(4.dp))
                Text(
                    text = "Bơm sẽ tự kích hoạt khi nước cạn và tự ngắt khi đầy",
                    fontSize = 13.sp,
                    color = TextSecondary
                )

                Spacer(modifier = Modifier.height(16.dp))

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(14.dp)
                ) {
                    // Ngưỡng Bật khi dưới 30%
                    Box(
                        modifier = Modifier
                            .weight(1f)
                            .clip(RoundedCornerShape(18.dp))
                            .background(SurfaceVariant)
                            .padding(14.dp)
                    ) {
                        Column {
                            Text(text = "Tự BẬT khi dưới", fontSize = 12.sp, color = TextSecondary)
                            Spacer(modifier = Modifier.height(6.dp))
                            Text(text = "30%", fontSize = 18.sp, fontWeight = FontWeight.Bold, color = BrandOrange)
                        }
                    }

                    // Ngưỡng Tắt khi đầy 95%
                    Box(
                        modifier = Modifier
                            .weight(1f)
                            .clip(RoundedCornerShape(18.dp))
                            .background(SurfaceVariant)
                            .padding(14.dp)
                    ) {
                        Column {
                            Text(text = "Tự TẮT khi đạt", fontSize = 12.sp, color = TextSecondary)
                            Spacer(modifier = Modifier.height(6.dp))
                            Text(text = "95%", fontSize = 18.sp, fontWeight = FontWeight.Bold, color = TextPrimary)
                        }
                    }
                }
            }
        }
    }
}
