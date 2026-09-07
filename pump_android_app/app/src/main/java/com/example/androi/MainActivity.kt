package com.example.androi

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.animation.Crossfade
import androidx.compose.animation.core.tween
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.Surface
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import com.example.androi.ui.navigation.MainAppContainer
import com.example.androi.ui.screens.splash.SplashScreen
import com.example.androi.ui.theme.AndroiTheme
import com.example.androi.ui.theme.ScreenBackground

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            AndroiTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = ScreenBackground
                ) {
                    var isSplashFinished by remember { mutableStateOf(false) }

                    Crossfade(
                        targetState = isSplashFinished,
                        animationSpec = tween(durationMillis = 600),
                        label = "SplashTransition"
                    ) { finished ->
                        if (!finished) {
                            SplashScreen(
                                onSplashComplete = {
                                    isSplashFinished = true
                                }
                            )
                        } else {
                            MainAppContainer()
                        }
                    }
                }
            }
        }
    }
}