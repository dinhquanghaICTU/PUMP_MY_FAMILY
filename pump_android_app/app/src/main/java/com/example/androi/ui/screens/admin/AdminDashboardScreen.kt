package com.example.androi.ui.screens.admin

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.example.androi.ui.theme.*

/**
 * Màn hình Admin Quản Trị Hệ Thống (Admin & Engineer Dashboard):
 * Thiết kế chuyên nghiệp, đồng bộ với kiến trúc dự án PUMP_MY_FAMILY:
 * - Giám sát phần cứng ESP32-S3 Master & Node Cảm Biến ESP-NOW Tầng Thượng
 * - Cấu hình thông số bể nước (Calibration & NVS Flash)
 * - Quản lý kết nối MQTT Broker (HiveMQ Cloud) & FastAPI Backend
 * - Terminal Log thời gian thực từ ESP32 & Nút can thiệp khẩn cấp (Reboot, OTA)
 */
@Composable
fun AdminDashboardScreen(
    onBackClick: () -> Unit = {},
    modifier: Modifier = Modifier
) {
    var selectedSection by remember { mutableStateOf(0) }
    val sections = listOf("Phần Cứng", "Hiệu Chuẩn Bể", "Mạng & MQTT", "Terminal Log")

    // State mẫu hiệu chuẩn bể
    var tankHeightCm by remember { mutableStateOf("150") }
    var blindZoneCm by remember { mutableStateOf("20") }
    var tankCapacityLit by remember { mutableStateOf("2000") }
    var isCalibSaved by remember { mutableStateOf(false) }

    LazyColumn(
        modifier = modifier
            .fillMaxSize()
            .background(ScreenBackground),
        contentPadding = PaddingValues(bottom = 120.dp)
    ) {
        // 1. Header Quản Trị Viên
        item {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 24.dp, vertical = 16.dp)
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column {
                        // Badge Admin màu cam
                        Box(
                            modifier = Modifier
                                .clip(RoundedCornerShape(8.dp))
                                .background(BrandOrange.copy(alpha = 0.15f))
                                .padding(horizontal = 10.dp, vertical = 4.dp)
                        ) {
                            Text(
                                text = "SYSTEM ADMINISTRATOR",
                                fontSize = 11.sp,
                                fontWeight = FontWeight.Bold,
                                color = BrandOrange,
                                letterSpacing = 0.5.sp
                            )
                        }

                        Spacer(modifier = Modifier.height(6.dp))

                        Text(
                            text = "Trung Tâm Quản Trị",
                            fontSize = 24.sp,
                            fontWeight = FontWeight.Black,
                            color = TextPrimary
                        )

                        Text(
                            text = "Kỹ sư điều hành: QUANG HÀ ICTU",
                            fontSize = 13.sp,
                            color = TextSecondary
                        )
                    }

                    // Avatar Admin / Shield Icon
                    Box(
                        modifier = Modifier
                            .size(48.dp)
                            .clip(CircleShape)
                            .background(BrandOrange),
                        contentAlignment = Alignment.Center
                    ) {
                        Icon(
                            imageVector = Icons.Rounded.AdminPanelSettings,
                            contentDescription = "Admin",
                            tint = Color.White,
                            modifier = Modifier.size(26.dp)
                        )
                    }
                }

                Spacer(modifier = Modifier.height(16.dp))

                // Thanh trạng thái kết nối Cloud / Network Status Pills
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    StatusBadge(
                        label = "FastAPI",
                        status = "24ms",
                        isOk = true,
                        modifier = Modifier.weight(1f)
                    )
                    StatusBadge(
                        label = "HiveMQ MQTT",
                        status = "TLS 8883",
                        isOk = true,
                        modifier = Modifier.weight(1f)
                    )
                    StatusBadge(
                        label = "ESP-NOW",
                        status = "2 Nodes",
                        isOk = true,
                        modifier = Modifier.weight(1f)
                    )
                }
            }
        }

        // 2. Tab chọn phân hệ quản trị
        item {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 24.dp),
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                sections.forEachIndexed { index, title ->
                    val isSelected = selectedSection == index
                    Box(
                        modifier = Modifier
                            .weight(1f)
                            .height(38.dp)
                            .clip(RoundedCornerShape(19.dp))
                            .background(if (isSelected) BrandOrange else CardBackground)
                            .clickable { selectedSection = index },
                        contentAlignment = Alignment.Center
                    ) {
                        Text(
                            text = title,
                            fontSize = 11.sp,
                            fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Medium,
                            color = if (isSelected) Color.White else TextPrimary
                        )
                    }
                }
            }
            Spacer(modifier = Modifier.height(16.dp))
        }

        // 3. Nội dung theo phân hệ được chọn
        when (selectedSection) {
            0 -> {
                // PHÂN HỆ 1: GIÁM SÁT PHẦN CỨNG & TELEMETRY
                item {
                    Column(
                        modifier = Modifier.padding(horizontal = 24.dp),
                        verticalArrangement = Arrangement.spacedBy(16.dp)
                    ) {
                        // Card 1: ESP32-S3 Master
                        AdminCard(
                            title = "Tủ Bơm Master (ESP32-S3)",
                            subtitle = "Xtensa LX7 Dual-Core 240MHz • FreeRTOS",
                            icon = Icons.Rounded.Memory
                        ) {
                            HardwareMetricRow("Bộ nhớ Internal SRAM", "512 KB (Khả dụng: 384 KB)")
                            HardwareMetricRow("Bộ nhớ Octal PSRAM", "8 MB N8R8 (Đang dùng: 1.4 MB)")
                            HardwareMetricRow("Bộ nhớ Flash SPI", "16 MB Octal Flash")
                            HardwareMetricRow("Nhiệt độ vi điều khiển", "41.8 °C (An toàn)")
                            HardwareMetricRow("Thời gian hoạt động (Uptime)", "14 ngày 08 giờ liên tục")
                            HardwareMetricRow("Trạng thái Relay Động Cơ", "Đang sẵn sàng • Tiếp điểm 10A")
                            HardwareMetricRow("Nguồn điện AC 220V", "226V - 50Hz (Ổn định)")

                            Spacer(modifier = Modifier.height(12.dp))

                            Row(
                                modifier = Modifier.fillMaxWidth(),
                                horizontalArrangement = Arrangement.spacedBy(10.dp)
                            ) {
                                Button(
                                    onClick = { /* TODO: Gửi lệnh Reboot ESP32 qua MQTT */ },
                                    colors = ButtonDefaults.buttonColors(containerColor = SurfaceVariant),
                                    shape = RoundedCornerShape(14.dp),
                                    modifier = Modifier.weight(1f)
                                ) {
                                    Icon(Icons.Rounded.RestartAlt, contentDescription = null, tint = TextPrimary, modifier = Modifier.size(16.dp))
                                    Spacer(modifier = Modifier.width(6.dp))
                                    Text("Reboot S3", fontSize = 12.sp, color = TextPrimary, fontWeight = FontWeight.SemiBold)
                                }

                                Button(
                                    onClick = { /* TODO: Kiểm tra OTA Firmware */ },
                                    colors = ButtonDefaults.buttonColors(containerColor = BrandOrange),
                                    shape = RoundedCornerShape(14.dp),
                                    modifier = Modifier.weight(1f)
                                ) {
                                    Icon(Icons.Rounded.SystemUpdate, contentDescription = null, tint = Color.White, modifier = Modifier.size(16.dp))
                                    Spacer(modifier = Modifier.width(6.dp))
                                    Text("Check OTA", fontSize = 12.sp, color = Color.White, fontWeight = FontWeight.SemiBold)
                                }
                            }
                        }

                        // Card 2: Node Cảm Biến Bể Nước (ESP-NOW Tầng Thượng)
                        AdminCard(
                            title = "Node Cảm Biến Tầng Thượng",
                            subtitle = "Giao thức ESP-NOW Peer-to-Peer • Năng lượng Solar",
                            icon = Icons.Rounded.Sensors
                        ) {
                            HardwareMetricRow("Địa chỉ MAC Node", "34:85:18:9B:2A:40")
                            HardwareMetricRow("Cảm biến mặt nước", "JSN-SR04T Chống nước IP67")
                            HardwareMetricRow("Chất lượng sóng RSSI", "-62 dBm (Kết nối rất tốt)")
                            HardwareMetricRow("Pin Lithium 18650", "4.15V (97%)")
                            HardwareMetricRow("Tấm thu năng lượng mặt trời", "Đang nạp: +380mA (Trời nắng)")
                            HardwareMetricRow("Chu kỳ đo & phát tin", "Mỗi 30s gửi 1 gói (Deep Sleep)")
                        }
                    }
                }
            }

            1 -> {
                // PHÂN HỆ 2: HIỆU CHUẨN THÔNG SỐ BỂ NƯỚC (CALIBRATION)
                item {
                    Column(
                        modifier = Modifier.padding(horizontal = 24.dp),
                        verticalArrangement = Arrangement.spacedBy(16.dp)
                    ) {
                        AdminCard(
                            title = "Hiệu Chuẩn Kích Thước Bể",
                            subtitle = "Lưu trực tiếp vào bộ nhớ Non-Volatile Storage (NVS) của ESP32",
                            icon = Icons.Rounded.Tune
                        ) {
                            Text(
                                text = "Thiết lập kích thước thực tế của bể inox gia đình để thuật toán tính toán chính xác số % và thể tích nước còn lại.",
                                fontSize = 12.sp,
                                color = TextSecondary,
                                lineHeight = 18.sp
                            )

                            Spacer(modifier = Modifier.height(14.dp))

                            OutlinedTextField(
                                value = tankHeightCm,
                                onValueChange = { tankHeightCm = it },
                                label = { Text("Chiều cao tổng thể bể (cm)") },
                                modifier = Modifier.fillMaxWidth(),
                                shape = RoundedCornerShape(16.dp),
                                singleLine = true
                            )

                            Spacer(modifier = Modifier.height(10.dp))

                            OutlinedTextField(
                                value = blindZoneCm,
                                onValueChange = { blindZoneCm = it },
                                label = { Text("Khoảng cách mù cảm biến (cm)") },
                                modifier = Modifier.fillMaxWidth(),
                                shape = RoundedCornerShape(16.dp),
                                singleLine = true
                            )

                            Spacer(modifier = Modifier.height(10.dp))

                            OutlinedTextField(
                                value = tankCapacityLit,
                                onValueChange = { tankCapacityLit = it },
                                label = { Text("Dung tích danh định của bể (Lít)") },
                                modifier = Modifier.fillMaxWidth(),
                                shape = RoundedCornerShape(16.dp),
                                singleLine = true
                            )

                            Spacer(modifier = Modifier.height(16.dp))

                            Button(
                                onClick = { isCalibSaved = true },
                                colors = ButtonDefaults.buttonColors(containerColor = BrandOrange),
                                shape = RoundedCornerShape(16.dp),
                                modifier = Modifier.fillMaxWidth()
                            ) {
                                Icon(Icons.Rounded.Save, contentDescription = null, tint = Color.White)
                                Spacer(modifier = Modifier.width(8.dp))
                                Text(
                                    text = if (isCalibSaved) "✓ Đã Lưu Vào NVS ESP32" else "Lưu Cấu Hình Vào Flash ESP32",
                                    fontSize = 14.sp,
                                    fontWeight = FontWeight.Bold,
                                    color = Color.White
                                )
                            }
                        }
                    }
                }
            }

            2 -> {
                // PHÂN HỆ 3: CẤU HÌNH MẠNG & MQTT BROKER
                item {
                    Column(
                        modifier = Modifier.padding(horizontal = 24.dp),
                        verticalArrangement = Arrangement.spacedBy(16.dp)
                    ) {
                        AdminCard(
                            title = "Thông Số Kết Nối Broker",
                            subtitle = "HiveMQ Cloud & FastAPI IoT Gateway",
                            icon = Icons.Rounded.CloudDone
                        ) {
                            HardwareMetricRow("MQTT Broker URL", "b492xxx.s1.eu.hivemq.cloud")
                            HardwareMetricRow("Cổng kết nối TLS", "8883 (Mã hóa SSL/TLS an toàn)")
                            HardwareMetricRow("Client ID", "ESP32S3_PUMP_MASTER_01")
                            HardwareMetricRow("Topic Lệnh (Pub)", "family_pump/cmd/relay")
                            HardwareMetricRow("Topic Trạng Thái (Sub)", "family_pump/telemetry/state")
                            HardwareMetricRow("FastAPI REST Endpoint", "http://192.168.1.100:8000/api/v1")
                            HardwareMetricRow("WebSocket Realtime", "ws://192.168.1.100:8000/ws/pump")

                            Spacer(modifier = Modifier.height(14.dp))

                            Button(
                                onClick = { /* TODO: Ping thử nghiệm */ },
                                colors = ButtonDefaults.buttonColors(containerColor = SurfaceVariant),
                                shape = RoundedCornerShape(16.dp),
                                modifier = Modifier.fillMaxWidth()
                            ) {
                                Icon(Icons.Rounded.NetworkCheck, contentDescription = null, tint = BrandOrange)
                                Spacer(modifier = Modifier.width(8.dp))
                                Text("Kiểm Tra Tốc Độ Phản Hồi (Ping Test)", color = TextPrimary, fontWeight = FontWeight.SemiBold, fontSize = 13.sp)
                            }
                        }
                    }
                }
            }

            3 -> {
                // PHÂN HỆ 4: TERMINAL LOG THỜI GIAN THỰC (SERIAL CONSOLE)
                item {
                    Column(
                        modifier = Modifier.padding(horizontal = 24.dp),
                        verticalArrangement = Arrangement.spacedBy(14.dp)
                    ) {
                        AdminCard(
                            title = "Nhật Ký Hệ Thống (Console Log)",
                            subtitle = "Luồng dữ liệu thời gian thực từ ESP32 qua WebSocket",
                            icon = Icons.Rounded.Terminal
                        ) {
                            // Khung hiển thị log phong cách Terminal đen
                            Box(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .height(260.dp)
                                    .clip(RoundedCornerShape(16.dp))
                                    .background(Color(0xFF1E242B))
                                    .padding(14.dp)
                            ) {
                                Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                                    Text("[09:44:02] [ESP32-S3] [I] Wi-Fi Connected. IP: 192.168.1.105", fontSize = 11.sp, color = Color(0xFF10B981), fontFamily = FontFamily.Monospace)
                                    Text("[09:44:03] [ESP32-S3] [I] MQTT Connected to HiveMQ Cloud (TLS 8883)", fontSize = 11.sp, color = Color(0xFF38BDF8), fontFamily = FontFamily.Monospace)
                                    Text("[09:44:05] [ESP-NOW] [I] Registered Tank Node Peer: 34:85:18:9B:2A:40", fontSize = 11.sp, color = Color(0xFFFBBF24), fontFamily = FontFamily.Monospace)
                                    Text("[09:44:35] [ESP-NOW] [D] Packet recv: dist=37.5cm, vbat=4.15V, rssi=-62", fontSize = 11.sp, color = Color.White, fontFamily = FontFamily.Monospace)
                                    Text("[09:44:35] [CALC] Water percent: 89.6% | Total volume: 1792 Liters", fontSize = 11.sp, color = Color(0xFF34D399), fontFamily = FontFamily.Monospace)
                                    Text("[09:45:00] [PUMP_CTRL] Auto Mode active. Water level safe. Standby.", fontSize = 11.sp, color = Color(0xFF94A3B8), fontFamily = FontFamily.Monospace)
                                    Text("[09:45:30] [ESP-NOW] [D] Packet recv: dist=37.6cm, vbat=4.15V, rssi=-63", fontSize = 11.sp, color = Color.White, fontFamily = FontFamily.Monospace)
                                }
                            }

                            Spacer(modifier = Modifier.height(10.dp))

                            Row(
                                modifier = Modifier.fillMaxWidth(),
                                horizontalArrangement = Arrangement.SpaceBetween,
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                Text("Trạng thái: Đang truyền dữ liệu (Live)", fontSize = 12.sp, color = Color(0xFF10B981))
                                Text("Tự cuộn theo log", fontSize = 12.sp, color = TextSecondary)
                            }
                        }
                    }
                }
            }
        }
    }
}

/**
 * Card bao bọc một khu vực thông số quản trị
 */
@Composable
private fun AdminCard(
    title: String,
    subtitle: String,
    icon: ImageVector,
    content: @Composable ColumnScope.() -> Unit
) {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(26.dp))
            .background(CardBackground)
            .padding(20.dp)
    ) {
        Column {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(12.dp)
            ) {
                Box(
                    modifier = Modifier
                        .size(42.dp)
                        .clip(CircleShape)
                        .background(SurfaceVariant),
                    contentAlignment = Alignment.Center
                ) {
                    Icon(
                        imageVector = icon,
                        contentDescription = null,
                        tint = BrandOrange,
                        modifier = Modifier.size(22.dp)
                    )
                }

                Column {
                    Text(
                        text = title,
                        fontSize = 16.sp,
                        fontWeight = FontWeight.Bold,
                        color = TextPrimary
                    )
                    Text(
                        text = subtitle,
                        fontSize = 12.sp,
                        color = TextSecondary
                    )
                }
            }

            Spacer(modifier = Modifier.height(16.dp))
            content()
        }
    }
}

/**
 * Hàng hiển thị một thông số kỹ thuật (Label & Value)
 */
@Composable
private fun HardwareMetricRow(label: String, value: String) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 5.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(text = label, fontSize = 13.sp, color = TextSecondary)
        Text(text = value, fontSize = 13.sp, fontWeight = FontWeight.SemiBold, color = TextPrimary)
    }
}

/**
 * Huy hiệu trạng thái kết nối
 */
@Composable
private fun StatusBadge(
    label: String,
    status: String,
    isOk: Boolean,
    modifier: Modifier = Modifier
) {
    Box(
        modifier = modifier
            .clip(RoundedCornerShape(16.dp))
            .background(CardBackground)
            .padding(vertical = 10.dp, horizontal = 10.dp),
        contentAlignment = Alignment.Center
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(
                    modifier = Modifier
                        .size(6.dp)
                        .clip(CircleShape)
                        .background(if (isOk) Color(0xFF10B981) else Color(0xFFEF4444))
                )
                Spacer(modifier = Modifier.width(5.dp))
                Text(
                    text = label,
                    fontSize = 11.sp,
                    fontWeight = FontWeight.Medium,
                    color = TextSecondary
                )
            }
            Spacer(modifier = Modifier.height(2.dp))
            Text(
                text = status,
                fontSize = 12.sp,
                fontWeight = FontWeight.Bold,
                color = TextPrimary
            )
        }
    }
}
