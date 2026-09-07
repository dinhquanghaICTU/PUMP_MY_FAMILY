package com.example.androi

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import com.example.androi.ui.navigation.MainAppContainer
import com.example.androi.ui.theme.AndroiTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            AndroiTheme {
                MainAppContainer()
            }
        }
    }
}