package com.example.androi.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Notifications
import androidx.compose.material.icons.rounded.Person
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.example.androi.ui.theme.BrandOrangeLight
import com.example.androi.ui.theme.BrandOrange
import com.example.androi.ui.theme.CardBackground
import com.example.androi.ui.theme.TextPrimary
import com.example.androi.ui.theme.TextSecondary

/**
 * Phần đầu trang (Header) với Avatar người dùng + Lời chào + Chuông thông báo
 */
@Composable
fun HeaderGreeting(
    userName: String = "Alex",
    greeting: String = "Chào buổi tối",
    onNotificationClick: () -> Unit = {},
    modifier: Modifier = Modifier
) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .padding(horizontal = 24.dp, vertical = 16.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            // Avatar tròn
            Box(
                modifier = Modifier
                    .size(46.dp)
                    .clip(CircleShape)
                    .background(BrandOrangeLight),
                contentAlignment = Alignment.Center
            ) {
                Icon(
                    imageVector = Icons.Rounded.Person,
                    contentDescription = "Avatar",
                    tint = BrandOrange,
                    modifier = Modifier.size(26.dp)
                )
            }

            Column {
                Text(
                    text = "Xin chào, $userName",
                    fontSize = 15.sp,
                    fontWeight = FontWeight.SemiBold,
                    color = TextPrimary
                )
                Text(
                    text = greeting,
                    fontSize = 13.sp,
                    color = TextSecondary
                )
            }
        }

        // Nút chuông thông báo hình tròn nền trắng
        Box(
            modifier = Modifier
                .size(44.dp)
                .clip(CircleShape)
                .background(CardBackground)
                .clickable { onNotificationClick() },
            contentAlignment = Alignment.Center
        ) {
            Icon(
                imageVector = Icons.Rounded.Notifications,
                contentDescription = "Notifications",
                tint = TextPrimary,
                modifier = Modifier.size(22.dp)
            )
        }
    }
}
