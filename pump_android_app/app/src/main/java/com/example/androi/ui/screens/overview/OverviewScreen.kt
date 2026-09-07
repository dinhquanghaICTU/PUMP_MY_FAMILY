package com.example.androi.ui.screens.overview

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.KeyboardArrowRight
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
import com.example.androi.ui.components.FamilyPhotoSlider
import com.example.androi.ui.components.HeaderGreeting
import com.example.androi.ui.components.SmartSwitch
import com.example.androi.ui.theme.*

/**
 * Màn hình Trang Chủ (Overview Screen):
 * - Phía trên: Slide ảnh gia đình vuốt ngang (FamilyPhotoSlider)
 * - Phía dưới: Ban đầu chỉ hiển thị ĐÚNG 1 ITEM thiết bị
 * - Khi click vào item thiết bị sẽ mở ra Màn hình Chi tiết (chứa trạng thái bể, khóa trẻ em, v.v.)
 */
@Composable
fun OverviewScreen(
    deviceState: DeviceUiState = DeviceUiState(),
    onTogglePump: (Boolean) -> Unit = {},
    onDeviceClick: (String) -> Unit = {},
    modifier: Modifier = Modifier
) {
    LazyColumn(
        modifier = modifier
            .fillMaxSize()
            .background(ScreenBackground),
        contentPadding = PaddingValues(bottom = 120.dp)
    ) {
        // 1. Header Lời chào & Avatar
        item {
            HeaderGreeting(
                userName = "Hà",
                greeting = "Chào gia đình nhỏ",
                onNotificationClick = { /* TODO: Mở thông báo */ }
            )
        }

        // 2. Slide ảnh của gia đình (Family Photo Slider)
        item {
            Spacer(modifier = Modifier.height(6.dp))
            FamilyPhotoSlider(
                onPhotoClick = { photoRes ->
                    /* TODO: Bạn có thể xử lý phóng to ảnh hoặc mở album */
                }
            )
            Spacer(modifier = Modifier.height(24.dp))
        }

        // 3. Tiêu đề mục "Thiết Bị Của Bạn" + Nút thêm thiết bị (+)
        item {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 24.dp),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column {
                    Text(
                        text = "Thiết Bị Của Bạn",
                        fontSize = 20.sp,
                        fontWeight = FontWeight.Bold,
                        color = TextPrimary
                    )
                    Text(
                        text = "1 thiết bị đang kết nối",
                        fontSize = 13.sp,
                        color = TextSecondary
                    )
                }

                Box(
                    modifier = Modifier
                        .size(40.dp)
                        .clip(CircleShape)
                        .background(BrandOrange)
                        .clickable { /* TODO: Thêm thiết bị mới */ },
                    contentAlignment = Alignment.Center
                ) {
                    Icon(
                        imageVector = Icons.Rounded.Add,
                        contentDescription = "Add device",
                        tint = Color.White,
                        modifier = Modifier.size(22.dp)
                    )
                }
            }
            Spacer(modifier = Modifier.height(16.dp))
        }

        // 4. Ban đầu chỉ 1 ITEM DUY NHẤT cho Thiết Bị
        // Click vào card này sẽ mở màn hình chi tiết (có trạng thái bể nước & khóa trẻ em)
        item {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 24.dp)
                    .clip(RoundedCornerShape(28.dp))
                    .background(CardBackground)
                    .clickable { onDeviceClick("main_pump") }
                    .padding(20.dp)
            ) {
                Column {
                    // Hàng trên: Icon + Tên thiết bị + Switch bật tắt nhanh
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            modifier = Modifier.weight(1f)
                        ) {
                            // Icon thiết bị tròn
                            Box(
                                modifier = Modifier
                                    .size(52.dp)
                                    .clip(CircleShape)
                                    .background(if (deviceState.isPumpRunning) BrandOrange else SurfaceVariant),
                                contentAlignment = Alignment.Center
                            ) {
                                Icon(
                                    imageVector = if (deviceState.isPumpRunning) Icons.Rounded.Waves else Icons.Rounded.WaterDrop,
                                    contentDescription = null,
                                    tint = if (deviceState.isPumpRunning) Color.White else BrandOrange,
                                    modifier = Modifier.size(28.dp)
                                )
                            }

                            Spacer(modifier = Modifier.width(16.dp))

                            Column {
                                Text(
                                    text = deviceState.name,
                                    fontSize = 17.sp,
                                    fontWeight = FontWeight.Bold,
                                    color = TextPrimary
                                )
                                Spacer(modifier = Modifier.height(3.dp))
                                Text(
                                    text = if (deviceState.isPumpRunning) "Máy đang bơm nước..." else "Tủ S3 Master • Sẵn sàng",
                                    fontSize = 13.sp,
                                    color = if (deviceState.isPumpRunning) BrandOrange else TextSecondary
                                )
                            }
                        }

                        // Switch nhanh ON / OFF
                        SmartSwitch(
                            checked = deviceState.isPumpRunning,
                            onCheckedChange = onTogglePump
                        )
                    }

                    Spacer(modifier = Modifier.height(18.dp))

                    // Hàng dưới: Thanh chỉ dẫn mở trang chi tiết
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(16.dp))
                            .background(SurfaceVariant)
                            .padding(horizontal = 14.dp, vertical = 10.dp),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Box(
                                modifier = Modifier
                                    .size(8.dp)
                                    .clip(CircleShape)
                                    .background(if (deviceState.isOnline) Color(0xFF10B981) else Color.Gray)
                            )
                            Spacer(modifier = Modifier.width(8.dp))
                            Text(
                                text = "Xem mức nước, khóa trẻ em & cài đặt",
                                fontSize = 12.sp,
                                fontWeight = FontWeight.Medium,
                                color = TextPrimary
                            )
                        }

                        Icon(
                            imageVector = Icons.AutoMirrored.Rounded.KeyboardArrowRight,
                            contentDescription = "Chi tiết",
                            tint = BrandOrange,
                            modifier = Modifier.size(20.dp)
                        )
                    }
                }
            }
        }
    }
}
