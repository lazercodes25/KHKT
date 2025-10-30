package com.example.connectesp

import android.animation.ObjectAnimator
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


class RoadListActivity : AppCompatActivity() {

    private val espBaseUrl = "http://192.168.4.1" // IP ESP32
    private lateinit var layoutRoads: LinearLayout
    private lateinit var rootLayout: LinearLayout
    private lateinit var prefs: android.content.SharedPreferences
    private val PREF_KEY_G0_NAME = "road_g0_name"
    private val PREF_KEY_G1_NAME = "road_g1_name"

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_road_list)

        layoutRoads = findViewById(R.id.layoutRoads)
        rootLayout = findViewById(R.id.rootLayout)
        prefs = getSharedPreferences("roads", Context.MODE_PRIVATE)
        loadRoads()

        // 🔁 Cập nhật trạng thái đèn liên tục 1s/lần
        val handler = android.os.Handler()
        val updateTask = object : Runnable {
            override fun run() {
                loadRoads()
                handler.postDelayed(this, 1000)
            }
        }
        handler.post(updateTask)
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
                    Snackbar.make(rootLayout, "⚠️ Lỗi đọc dữ liệu từ ESP32!", Snackbar.LENGTH_SHORT).show()
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
        val userPrefs = getSharedPreferences("user", MODE_PRIVATE)
        var roadID2 = 0
        if (roadId == 0) roadID2 = 1 else {
            roadID2 = 0
        }
        val speed = userPrefs.getInt("speed", 30)

        val json = """{"roadId":${roadID2 + 1}, "speed":$speed}""" // ESP32 dùng roadId = 1 hoặc 2
        EspApiClient.postJson("/set_road", json) { response ->
            runOnUiThread {
                try {
                    if (response.contains("\"status\":\"ok\"")) {
                        // ✅ Giải mã JSON phản hồi
                        val jsonRes = JSONObject(response)
                        val wait = jsonRes.optInt("wait_seconds", 0)
                        val bonus = jsonRes.optInt("bonus_seconds", 0)

                        // 🧩 Hiển thị hộp thoại kết quả
                        showPriorityDialog(wait, bonus)
                        //
                    }
                    else if (response.contains("Another road")) {
                        Snackbar.make(rootLayout, "⏳ Đang chờ đường khác...", Snackbar.LENGTH_SHORT).show()
                    }
                    else if (response.startsWith("❌")) {
                        Snackbar.make(rootLayout, response, Snackbar.LENGTH_SHORT).show()
                    }
                    else {
                        Snackbar.make(rootLayout, "⚠️ Không xác định: $response", Snackbar.LENGTH_SHORT).show()
                    }
                } catch (e: Exception) {
                    Snackbar.make(rootLayout, "⚠️ Lỗi xử lý phản hồi!", Snackbar.LENGTH_SHORT).show()
                }
            }
        }
    }
    // 💬 Hộp thoại hiển thị thời gian ưu tiên
    private fun showPriorityDialog(wait: Int, bonus: Int) {
        val view = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(48, 32, 48, 32)
            addView(TextView(context).apply {
                text = "⏳ Thời gian chờ: ${wait}s"
                textSize = 18f
                setTextColor(Color.BLACK)
            })
            addView(TextView(context).apply {
                text = "🚦 Thời gian đèn đỏ ưu tiên: ${bonus}s"
                textSize = 18f
                setTextColor(Color.BLACK)
                setPadding(0, 16, 0, 0)
            })
        }

        com.google.android.material.dialog.MaterialAlertDialogBuilder(this)
            .setTitle("Ưu tiên được chấp nhận ✅")
            .setView(view)
            .setPositiveButton("Đóng") { _, _ ->
                // 🔙 Trở về màn SsidMenu khi nhấn "Đóng"
                val intent = android.content.Intent(this, SsidMenu::class.java)
                intent.addFlags(android.content.Intent.FLAG_ACTIVITY_CLEAR_TOP or android.content.Intent.FLAG_ACTIVITY_NEW_TASK)
                startActivity(intent)
                finish()
            }
            .show()
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
