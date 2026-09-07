package com.example.androi.ui.components

import androidx.compose.animation.animateColorAsState
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.CalendarMonth
import androidx.compose.material.icons.rounded.GridView
import androidx.compose.material.icons.rounded.Home
import androidx.compose.material.icons.rounded.Person
import androidx.compose.material3.Icon
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.unit.dp
import com.example.androi.ui.theme.BrandOrange
import com.example.androi.ui.theme.DarkPillBackground
import com.example.androi.ui.theme.DarkPillInactiveIcon

enum class NavigationTab(val title: String, val icon: ImageVector) {
    HOME("Tổng quan", Icons.Rounded.Home),
    SCHEDULE("Lịch hẹn", Icons.Rounded.CalendarMonth),
    DEVICES("Thiết bị", Icons.Rounded.GridView),
    PROFILE("Cá nhân", Icons.Rounded.Person)
}

/**
 * Thanh điều hướng nổi (Floating Bottom Bar) màu đen bo tròn
 * với nút Tab đang chọn có nền tròn màu Cam nổi bật theo đúng mẫu thiết kế.
 */
@Composable
fun FloatingBottomBar(
    currentTab: NavigationTab,
    onTabSelected: (NavigationTab) -> Unit,
    modifier: Modifier = Modifier
) {
    Box(
        modifier = modifier
            .fillMaxWidth()
            .padding(horizontal = 40.dp, vertical = 20.dp),
        contentAlignment = Alignment.Center
    ) {
        Row(
            modifier = Modifier
                .shadow(
                    elevation = 16.dp,
                    shape = RoundedCornerShape(36.dp),
                    spotColor = Color.Black.copy(alpha = 0.35f)
                )
                .clip(RoundedCornerShape(36.dp))
                .background(DarkPillBackground)
                .padding(horizontal = 10.dp, vertical = 8.dp),
            horizontalArrangement = Arrangement.spacedBy(16.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            NavigationTab.values().forEach { tab ->
                val isSelected = currentTab == tab
                val interactionSource = remember { MutableInteractionSource() }

                val iconColor by animateColorAsState(
                    targetValue = if (isSelected) Color.White else DarkPillInactiveIcon,
                    label = "tabIconColor"
                )

                Box(
                    modifier = Modifier
                        .size(48.dp)
                        .clip(CircleShape)
                        .background(if (isSelected) BrandOrange else Color.Transparent)
                        .clickable(
                            interactionSource = interactionSource,
                            indication = null
                        ) {
                            onTabSelected(tab)
                        },
                    contentAlignment = Alignment.Center
                ) {
                    Icon(
                        imageVector = tab.icon,
                        contentDescription = tab.title,
                        tint = iconColor,
                        modifier = Modifier.size(24.dp)
                    )
                }
            }
        }
    }
}
