package com.example.connectesp

import android.Manifest
import android.animation.ObjectAnimator
import android.annotation.SuppressLint
import android.graphics.Color
import android.os.Bundle
import android.view.Gravity
import android.widget.LinearLayout
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import com.google.android.material.card.MaterialCardView
import com.google.android.material.snackbar.Snackbar
import org.json.JSONObject
import android.content.Context
import android.content.Intent
import android.os.CountDownTimer
import android.graphics.Typeface
import android.media.MediaPlayer
import android.os.Vibrator
import android.os.VibrationEffect
import android.os.Build
import android.view.animation.AlphaAnimation
import android.view.animation.Animation
import androidx.annotation.RequiresPermission
import android.os.Looper
import android.os.Handler
import android.widget.Button
import java.io.File

class RoadListActivity : AppCompatActivity() {

    private val espBaseUrl = "http://192.168.4.1" // IP ESP32
    private lateinit var layoutRoads: LinearLayout
    private lateinit var rootLayout: LinearLayout
    private lateinit var prefs: android.content.SharedPreferences
    private val PREF_KEY_G0_NAME = "road_g0_name"
    private val PREF_KEY_G1_NAME = "road_g1_name"
    private var updateHandler: Handler? = null
    private var updateRunnable: Runnable? = null

    private var isConnected = false
    private val handler = Handler(Looper.getMainLooper())
    private var updateTaskRunning = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_road_list)

        layoutRoads = findViewById(R.id.layoutRoads)
        rootLayout = findViewById(R.id.rootLayout)
        prefs = getSharedPreferences("roads", Context.MODE_PRIVATE)
        val tvUserName = findViewById<TextView>(R.id.tvUserName)
        val btnLogout = findViewById<Button>(R.id.btnLogout)
        val layoutRoads = findViewById<LinearLayout>(R.id.layoutRoads)

        // Lấy thông tin user
        val prefs1 = getSharedPreferences("user", MODE_PRIVATE)
        val displayName = prefs1.getString("name", "Người dùng")
        tvUserName.text = displayName
        // Nút đăng xuất
        btnLogout.setOnClickListener {
            val file = File(filesDir, "login.json")
            if (file.exists()) file.delete()
            prefs1.edit().clear().apply()
            val intent = Intent(this, LoginActivity::class.java)
            intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
            startActivity(intent)
            finish()
        }
        // 🛰️ Bắt đầu auto-check kết nối với ESP32 mỗi 3 giây
        EspApiClient.startAutoCheck(
            onConnected = {
                if (!isConnected) {
                    isConnected = true
                    startUpdateTask()
                }
            },
            onDisconnected = { msg ->
                if (isConnected) {
                    isConnected = false
                    Snackbar.make(rootLayout, msg, Snackbar.LENGTH_LONG).show()
                    stopUpdateTask()
                }
            }
        )
    }
    override fun onDestroy() {
        super.onDestroy()
        EspApiClient.stopAutoCheck()
        handler.removeCallbacksAndMessages(null)
        updateTaskRunning = false
    }
    // Bắt đầu load trạng thái đèn mỗi giây
    private fun startUpdateTask() {
        if (updateTaskRunning) return
        updateTaskRunning = true
        val updateTask = object : Runnable {
            override fun run() {
                if (isConnected) {
                    loadRoads()
                    handler.postDelayed(this, 1000)
                } else {
                    updateTaskRunning = false
                }
            }
        }
        handler.post(updateTask)
    }

    // Dừng cập nhật
    private fun stopUpdateTask() {
        handler.removeCallbacksAndMessages(null)
        updateTaskRunning = false
        // Mở dialog thông báo Thiết bị đã bị ngắt kết nối nếu nhấn đóng sẽ trở lại SsidMenu
        showDisconnectedDialog() // ⚡ Hiển thị cảnh báo
    }


    private fun stopUpdate() {
        updateHandler?.removeCallbacks(updateRunnable ?: return)
        updateHandler = null
        updateRunnable = null
    }
    // 🛰️ Lấy trạng thái đèn từ ESP32
    private fun loadRoads() {
        EspApiClient.get("/status") { response ->
            try {
                val json = JSONObject(response)

                val g0 = json.getJSONObject("G0")
                val g1 = json.getJSONObject("G1")
                // 🧠 Lấy tên từ server hoặc SharedPreferences nếu không có
                val name0 = g0.optString("name", prefs.getString(PREF_KEY_G0_NAME, "Bắc – Nam") ?: "Bắc – Nam")
                val name1 = g1.optString("name", prefs.getString(PREF_KEY_G1_NAME, "Đông – Tây") ?: "Đông – Tây")

                // 💾 Lưu lại nếu server trả tên khác
                prefs.edit().apply {
                    putString(PREF_KEY_G0_NAME, name0)
                    putString(PREF_KEY_G1_NAME, name1)
                    apply()
                }

                val roads = listOf(
                    Road(0, g0.getString("name"), g0.getString("phase"), g0.getInt("remaining")),
                    Road(1, g1.getString("name"), g1.getString("phase"), g1.getInt("remaining"))
                )

                runOnUiThread { showRoadCards(roads) }

            } catch (e: Exception) {
                runOnUiThread {
                    Snackbar.make(rootLayout, "Đã mất kết nối với Đèn giao thông", Snackbar.LENGTH_SHORT).show()
                }
            }
        }
    }

    // 🧱 Hiển thị card đường
    private fun showRoadCards(roads: List<Road>) {
        layoutRoads.removeAllViews()

        for (road in roads) {
            val card = MaterialCardView(this).apply {
                radius = 28f
                cardElevation = 6f
                setContentPadding(24, 32, 24, 32)
                useCompatPadding = true
                strokeWidth = 3
                strokeColor = Color.parseColor("#DADCE0")
                layoutParams = LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
                ).apply { setMargins(0, 16, 0, 0) }

                // 🎨 Chọn màu nền theo pha đèn
                setCardBackgroundColor(
                    when (road.phase) {
                        "RED" -> Color.parseColor("#EF9A9A")   // đỏ nhạt
                        "YELLOW" -> Color.parseColor("#FFF59D") // vàng
                        "GREEN" -> Color.parseColor("#A5D6A7")  // xanh
                        else -> Color.parseColor("#E0E0E0")     // mặc định
                    }
                )
            }

            // 🕒 Text hiển thị tên + pha + thời gian
            val tv = TextView(this).apply {
                text = "🚦 ${road.name}\n${road.phase} - còn ${road.remaining}s"
                textSize = 18f
                setTextColor(Color.BLACK)
                gravity = Gravity.CENTER
            }

            card.addView(tv)
            layoutRoads.addView(card)

            card.setOnClickListener {
                ObjectAnimator.ofFloat(card, "alpha", 0.7f, 1f).apply {
                    duration = 200
                    start()
                }
                sendSetRoad(road.id)
            }
        }
    }

    // 🚦 Gửi yêu cầu ưu tiên đường
    private fun sendSetRoad(roadId: Int) {
        val prefs = getSharedPreferences("user", MODE_PRIVATE)
        val speed = prefs.getFloat("speed", 1.0F)
        val roadIdEsp = if (roadId == 0) 2 else 1 // ESP dùng 1 hoặc 2

        val json = """{"roadId":$roadIdEsp, "speed":${speed.toDouble()}}"""

        EspApiClient.postJson("/set_road", json) { response ->
            runOnUiThread {
                try {
                    val jsonRes = JSONObject(response)
                    val status = jsonRes.optString("status")
                    when (status) {
                        "ok" -> {
                            val wait = jsonRes.optInt("wait_seconds")
                            val bonus = jsonRes.optInt("bonus_seconds")
                            val oppRemaining = jsonRes.optInt("oppRemaining", -1)
                            val redCase = jsonRes.optInt("RedCase", 0)
                            showPriorityDialog(wait, bonus,oppRemaining, redCase == 1)
                        }
                        "speed not higher than previous" ->
                        {
                            Snackbar.make(rootLayout, "Đã gửi yêu cầu ưu tiên.", Snackbar.LENGTH_SHORT).show()
                            val intent = Intent(this, SsidMenu::class.java)
                            intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_NEW_TASK)
                            startActivity(intent)
                            finish()
                        }
                        else -> Snackbar.make(rootLayout, "⚠️ Không xác định: $response", Snackbar.LENGTH_SHORT).show()
                    }
                } catch (e: Exception) {
                    Snackbar.make(rootLayout, "⚠️ Lỗi xử lý phản hồi!", Snackbar.LENGTH_SHORT).show()
                }
            }
        }
    }
    // 💬 Hộp thoại hiển thị thời gian ưu tiên
    @SuppressLint("SetTextI18n")
    private fun showPriorityDialog(wait: Int, bonus: Int, oppRemaining: Int, isRedCase: Boolean) {
        // Phát hiện dark mode
        val isDarkMode = (resources.configuration.uiMode and
                android.content.res.Configuration.UI_MODE_NIGHT_MASK) ==
                android.content.res.Configuration.UI_MODE_NIGHT_YES

        val textColor = if (isDarkMode) Color.WHITE else Color.BLACK
        val bgColor = if (isDarkMode) Color.parseColor("#2C2C2C") else Color.parseColor("#FAFAFA")

        // Layout chính
        val view = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(48, 32, 48, 32)
            setBackgroundColor(bgColor)
            gravity = Gravity.CENTER_HORIZONTAL
        }

        // Text thời gian chờ
        val waitText = TextView(this).apply {
            text = "⏳ Thời gian chờ: ${wait}s"
            textSize = 20f
            setTextColor(textColor)
            typeface = Typeface.DEFAULT_BOLD
            gravity = Gravity.CENTER
        }

        // Text bonus time
//        val bonusText = TextView(this).apply {
//            text = "🚦 Thời gian đèn đỏ ưu tiên : ${bonus}s"
//            textSize = 18f
//            setTextColor(textColor)
//            setPadding(0, 16, 0, 0)
//            gravity = Gravity.CENTER
//        }
        view.addView(waitText)
        //view.addView(bonusText)

        // Tạo dialog
        val dialog = com.google.android.material.dialog.MaterialAlertDialogBuilder(this)
            .setTitle("Ưu tiên được chấp nhận ✅")
            .setView(view)
            .setCancelable(false)
            .setPositiveButton("Đóng") { _, _ ->
                val intent = Intent(this, SsidMenu::class.java)
                intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_NEW_TASK)
                startActivity(intent)
                finish()
            }
            .create()

        dialog.show()

        // Countdown timer
        object : CountDownTimer(wait * 1000L, 1000L) {
            override fun onTick(millisUntilFinished: Long) {
                val secondsLeft = (millisUntilFinished / 1000).toInt()
                waitText.text = "⏳ Thời gian chờ: ${secondsLeft}s"
                // 🔥 DÒNG NÀY LÀ NƠI BẮT ĐẦU UPDATE
                if (isRedCase && (wait - secondsLeft) >= oppRemaining + 1) {
                    EspApiClient.get("/update") { apiRes ->
                        try {
                            val json = JSONObject(apiRes)
                            val newGreenTime = json.optInt("green_time", secondsLeft)

                            // Nếu server trả thời gian khác → cập nhật timer
                            if (newGreenTime != secondsLeft) {
                                runOnUiThread {
                                    waitText.text = "⏳ Thời gian chờ: ${newGreenTime}s"
                                }

                                // Reset timer
                                cancel()
                                startNewTimer(newGreenTime, bonus, waitText, dialog)
                            }

                        } catch (_: Exception) {}
                    }
                }
            }

            private fun startNewTimer(newWait: Int, bonus: Int, waitText: TextView, dialog: androidx.appcompat.app.AlertDialog) {
                object : CountDownTimer((newWait + 3) * 1000L, 900L) {
                    override fun onTick(millisUntilFinished: Long) {
                        val secondsLeft = (millisUntilFinished / 1000).toInt()
                        waitText.text = "⏳ Thời gian chờ: ${secondsLeft}s"
                    }
                    @RequiresPermission(Manifest.permission.VIBRATE)
                    @SuppressLint("ServiceCast", "SuspiciousIndentation")
                    override fun onFinish() {
                        waitText.text = "✅ Đã đến lượt ưu tiên!"
                        waitText.setTextColor(Color.parseColor("#E91E63"))
                        waitText.textSize = 22f
                        // 🔊 Phát âm thanh thông báo
                        try {
                            val mediaPlayer = MediaPlayer.create(this@RoadListActivity, R.raw.priority_alert)
                            mediaPlayer?.start()
                            mediaPlayer?.setOnCompletionListener {
                                it.release()
                            }
                        } catch (e: Exception) {
                            e.printStackTrace()
                        }
                        // Rung nhẹ 300ms
                        // 💥 Rung lặp từng đợt cho đến khi dialog đóng
                        val vibrator = getSystemService(Context.VIBRATOR_SERVICE) as Vibrator

                        // Mẫu rung: [rung, nghỉ, rung, nghỉ, ...]
                        val pattern = longArrayOf(0, 500, 300, 500, 300) // ms

                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                            val effect = VibrationEffect.createWaveform(pattern, 3) // 0 = lặp vô hạn
                            vibrator.vibrate(effect)
                        } else {
                            @Suppress("DEPRECATION")
                            vibrator.vibrate(pattern, 6) // lặp vô hạn trên Android cũ
                        }

                        // 🧩 Dừng rung khi dialog bị đóng
                        dialog.setOnDismissListener {
                            vibrator.cancel()
                        }
                    }
                }.start()
            }

            @RequiresPermission(Manifest.permission.VIBRATE)
            @SuppressLint("ServiceCast", "SuspiciousIndentation")
            override fun onFinish() {
                waitText.text = "✅ Đã đến lượt ưu tiên!"
                waitText.setTextColor(Color.parseColor("#E91E63")) // Hồng nổi bật
                waitText.textSize = 22f
                // 🔊 Phát âm thanh thông báo
                try {
                    val mediaPlayer = MediaPlayer.create(this@RoadListActivity, R.raw.priority_alert)
                    mediaPlayer?.start()
                    mediaPlayer?.setOnCompletionListener {
                        it.release()
                    }
                } catch (e: Exception) {
                    e.printStackTrace()
                }
                // Rung nhẹ 300ms
                // 💥 Rung lặp từng đợt cho đến khi dialog đóng
                val vibrator = getSystemService(Context.VIBRATOR_SERVICE) as Vibrator

                // Mẫu rung: [rung, nghỉ, rung, nghỉ, ...]
                val pattern = longArrayOf(0, 500, 300, 500, 300) // ms

                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    val effect = VibrationEffect.createWaveform(pattern, 3) // 0 = lặp vô hạn
                    vibrator.vibrate(effect)
                } else {
                    @Suppress("DEPRECATION")
                    vibrator.vibrate(pattern, 6) // lặp vô hạn trên Android cũ
                }

                // 🧩 Dừng rung khi dialog bị đóng
                dialog.setOnDismissListener {
                    vibrator.cancel()
                }
            }
        }.start()
    }
    private fun showDisconnectedDialog() {
        runOnUiThread {
            val builder = androidx.appcompat.app.AlertDialog.Builder(this)
            builder.setTitle("⚠️ Mất kết nối thiết bị")
            builder.setMessage("Thiết bị đèn giao thông đã bị ngắt kết nối.\nBạn sẽ trở lại màn hình chọn Đèn.")

            builder.setCancelable(false)
            builder.setPositiveButton("Đóng") { _, _ ->

                // Quay lại màn hình chọn SSID
                val intent = Intent(this, SsidMenu::class.java)
                intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_NEW_TASK)
                startActivity(intent)
                finish()
            }

            val dialog = builder.create()
            if (!isFinishing && !isDestroyed) dialog.show()
        }
    }



    // Model nhỏ cho dễ quản lý
    // 🧩 Model
    data class Road(
        val id: Int,
        val name: String,
        val phase: String,
        val remaining: Int
    )
}
