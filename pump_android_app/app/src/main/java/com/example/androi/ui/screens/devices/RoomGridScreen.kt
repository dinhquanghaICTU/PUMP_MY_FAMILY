package com.example.androi.ui.screens.devices

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
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
import com.example.androi.data.model.DeviceCardItem
import com.example.androi.data.model.DeviceIconType
import com.example.androi.ui.components.DeviceGridCard
import com.example.androi.ui.theme.*

/**
 * Màn hình 2: Lưới thiết bị 2x2 theo khu vực (Screen 2 - Living Room / Devices Grid)
 * Đã dựng sẵn UI 100% khớp template để bạn tự xử lý sự kiện
 */
@Composable
fun RoomGridScreen(
    roomName: String = "Tủ Bơm & Bể Nước",
    onBackClick: () -> Unit = {},
    onDeviceClick: (String) -> Unit = {},
    modifier: Modifier = Modifier
) {
    var isPumpRunning by remember { mutableStateOf(true) }
    var isChildLock by remember { mutableStateOf(false) }
    var isSolarActive by remember { mutableStateOf(true) }

    val devices = remember(isPumpRunning, isChildLock, isSolarActive) {
        listOf(
            DeviceCardItem("pump", "Máy Bơm Chính", "Tủ Điện S3", DeviceIconType.PUMP, isPumpRunning, if (isPumpRunning) "Đang Bật" else "Đang Tắt"),
            DeviceCardItem("tank", "Cảm Biến Bể", "Sóng ESP-NOW", DeviceIconType.WATER_TANK, true, "Nước: 89.6%"),
            DeviceCardItem("lock", "Khóa An Toàn", "Khóa nút vật lý", DeviceIconType.CHILD_LOCK, isChildLock, if (isChildLock) "ĐANG KHÓA" else "MỞ KHÓA"),
            DeviceCardItem("solar", "Nguồn Pin Solar", "Bể Nước", DeviceIconType.SOLAR_BATTERY, isSolarActive, "Pin: 4.15V")
        )
    }

    Column(
        modifier = modifier
            .fillMaxSize()
            .background(ScreenBackground)
            .padding(bottom = 100.dp)
    ) {
        // 1. Top Bar: Nút Back tròn + Tiêu đề giữa
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

            Text(
                text = roomName,
                fontSize = 18.sp,
                fontWeight = FontWeight.Bold,
                color = TextPrimary
            )

            // Khoảng trống cân đối góc phải
            Spacer(modifier = Modifier.size(44.dp))
        }

        // 2. Banner Card khu vực (Hình minh họa + Thanh nút nổi)
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 24.dp)
                .height(170.dp)
                .clip(RoundedCornerShape(32.dp))
                .background(Color(0xFFE3E8EC)),
            contentAlignment = Alignment.Center
        ) {
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Icon(
                    imageVector = Icons.Rounded.HomeWork,
                    contentDescription = "Room Banner",
                    tint = BrandOrange.copy(alpha = 0.7f),
                    modifier = Modifier.size(56.dp)
                )
                Spacer(modifier = Modifier.height(6.dp))
                Text(
                    text = "Hệ thống Bơm & Bể Nước Gia Đình",
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Medium,
                    color = TextPrimary
                )
            }

            // Thanh viên thuốc mờ nổi ở đáy banner
            Row(
                modifier = Modifier
                    .align(Alignment.BottomCenter)
                    .padding(bottom = 16.dp)
                .clip(RoundedCornerShape(24.dp))
                .background(Color.White.copy(alpha = 0.85f))
                .padding(horizontal = 14.dp, vertical = 6.dp),
                horizontalArrangement = Arrangement.spacedBy(16.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Icon(Icons.Rounded.Videocam, contentDescription = null, tint = TextPrimary, modifier = Modifier.size(18.dp))
                Icon(Icons.Rounded.CropFree, contentDescription = null, tint = TextPrimary, modifier = Modifier.size(18.dp))
                Icon(Icons.Rounded.RadioButtonChecked, contentDescription = null, tint = BrandOrange, modifier = Modifier.size(18.dp))
            }
        }

        Spacer(modifier = Modifier.height(24.dp))

        // 3. Tiêu đề "Thiết bị của tôi" (My Devices) + "Xem tất cả"
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 24.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = "Thiết bị của tôi",
                fontSize = 18.sp,
                fontWeight = FontWeight.Bold,
                color = TextPrimary
            )
            Text(
                text = "Xem tất cả",
                fontSize = 13.sp,
                color = TextSecondary,
                modifier = Modifier.clickable { /* TODO: Xem tất cả */ }
            )
        }

        Spacer(modifier = Modifier.height(14.dp))

        // 4. Lưới 2x2 Thiết bị (2 columns)
        LazyVerticalGrid(
            columns = GridCells.Fixed(2),
            modifier = Modifier
                .fillMaxSize()
                .padding(horizontal = 20.dp),
            horizontalArrangement = Arrangement.spacedBy(14.dp),
            verticalArrangement = Arrangement.spacedBy(14.dp)
        ) {
            items(devices.size) { index ->
                val device = devices[index]
                DeviceGridCard(
                    item = device,
                    onCardClick = { onDeviceClick(device.id) },
                    onToggle = { newState ->
                        when (device.id) {
                            "pump" -> isPumpRunning = newState
                            "lock" -> isChildLock = newState
                            "solar" -> isSolarActive = newState
                        }
                    }
                )
            }
        }
    }
}
