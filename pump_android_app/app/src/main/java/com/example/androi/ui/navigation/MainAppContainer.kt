package com.example.androi.ui.navigation

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import com.example.androi.data.model.DeviceUiState
import com.example.androi.ui.components.FloatingBottomBar
import com.example.androi.ui.components.NavigationTab
import com.example.androi.ui.screens.admin.AdminDashboardScreen
import com.example.androi.ui.screens.detail.DetailControlScreen
import com.example.androi.ui.screens.devices.RoomGridScreen
import com.example.androi.ui.screens.overview.OverviewScreen

/**
 * Container chính bao bọc toàn bộ ứng dụng:
 * - Điều hướng mượt mà: Trang chủ -> Chi tiết bể/bơm -> Danh sách thiết bị -> Quản trị Admin
 * - Hiển thị FloatingBottomBar màu đen nổi ở đáy màn hình
 * - Để sẵn biến deviceUiState để bạn gắn ViewModel / API sau này
 */
@Composable
fun MainAppContainer() {
    var currentTab by remember { mutableStateOf(NavigationTab.HOME) }
    var activeScreen by remember { mutableStateOf("OVERVIEW") } // "OVERVIEW", "ROOM_GRID", "DETAIL", "ADMIN"

    // Trạng thái thiết bị mẫu (bạn có thể thay bằng ViewModel sau này)
    var deviceUiState by remember {
        mutableStateOf(
            DeviceUiState(
                name = "Tủ Bơm Nước Gia Đình",
                isPumpRunning = false,
                isAutoMode = true,
                isChildLock = false,
                waterPercent = 89.6f,
                distanceCm = 37.5f,
                batteryVoltage = 4.15f
            )
        )
    }

    Box(modifier = Modifier.fillMaxSize()) {
        // 1. Hiển thị nội dung màn hình tương ứng
        when (activeScreen) {
            "OVERVIEW" -> {
                OverviewScreen(
                    deviceState = deviceUiState,
                    onTogglePump = { newState ->
                        deviceUiState = deviceUiState.copy(isPumpRunning = newState)
                    },
                    onDeviceClick = { deviceId ->
                        activeScreen = "DETAIL"
                    }
                )
            }
            "ROOM_GRID" -> {
                RoomGridScreen(
                    roomName = "Tủ Bơm & Bể Nước",
                    onBackClick = {
                        activeScreen = "OVERVIEW"
                        currentTab = NavigationTab.HOME
                    },
                    onDeviceClick = { deviceId ->
                        activeScreen = "DETAIL"
                    }
                )
            }
            "DETAIL" -> {
                DetailControlScreen(
                    deviceState = deviceUiState,
                    onTogglePump = { newState ->
                        deviceUiState = deviceUiState.copy(isPumpRunning = newState)
                    },
                    onToggleChildLock = { newState ->
                        deviceUiState = deviceUiState.copy(isChildLock = newState)
                    },
                    onToggleAutoMode = { isAuto ->
                        deviceUiState = deviceUiState.copy(isAutoMode = isAuto)
                    },
                    onBackClick = {
                        activeScreen = "OVERVIEW"
                        currentTab = NavigationTab.HOME
                    }
                )
            }
            "ADMIN" -> {
                AdminDashboardScreen(
                    onBackClick = {
                        activeScreen = "OVERVIEW"
                        currentTab = NavigationTab.HOME
                    }
                )
            }
        }

        // 2. Thanh điều hướng nổi (Floating Bottom Bar)
        FloatingBottomBar(
            currentTab = currentTab,
            onTabSelected = { selectedTab ->
                currentTab = selectedTab
                activeScreen = when (selectedTab) {
                    NavigationTab.HOME -> "OVERVIEW"
                    NavigationTab.SCHEDULE -> "DETAIL"
                    NavigationTab.DEVICES -> "ROOM_GRID"
                    NavigationTab.ADMIN -> "ADMIN"
                }
            },
            modifier = Modifier.align(Alignment.BottomCenter)
        )
    }
}
