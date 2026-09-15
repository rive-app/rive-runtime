package app.rive.android_tests

import android.app.NativeActivity
import android.app.UiModeManager
import android.content.Context
import android.content.pm.ActivityInfo
import android.content.res.Configuration
import android.os.Bundle

// Base class for the android test activities.
open class RiveNativeActivity : NativeActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val uiModeManager =
            getSystemService(Context.UI_MODE_SERVICE) as UiModeManager
        if (uiModeManager.currentModeType ==
            Configuration.UI_MODE_TYPE_TELEVISION) {
            // TVs are fixed landscape panels, so our default portrait
            // orientation gets letterboxed into a vertical strip. This
            // overrides to landscape on TVs and leaves phones in portrait.
            requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
        }
    }
}
