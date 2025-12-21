package com.example.connectesp

import android.content.Intent
import android.os.Bundle
import android.widget.Button
import android.widget.EditText
import com.google.android.material.snackbar.Snackbar
import androidx.appcompat.app.AppCompatActivity
import org.json.JSONObject
import java.nio.charset.Charset
import java.io.File


class LoginActivity : AppCompatActivity() {
    private lateinit var edtUsername: EditText
    private lateinit var edtPassword: EditText
    private lateinit var btnLogin: Button

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_login)
        // ✅ Kiểm tra auto login từ login.json
        val savedUser = readLoginInfo()
        if (savedUser != null) {
            val username = savedUser.getString("username")
            val speed = savedUser.getDouble("speed")
            startActivity(Intent(this, SsidMenu::class.java))
            overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out)
            finish()
            return
        }
        edtUsername = findViewById(R.id.edtUsername)
        edtPassword = findViewById(R.id.edtPassword)
        btnLogin = findViewById(R.id.btnLogin)

        btnLogin.setOnClickListener {
            val username = edtUsername.text.toString().trim()
            val password = edtPassword.text.toString().trim()

            if (username.isEmpty() || password.isEmpty()) {
                Snackbar.make(btnLogin, "⚠️ Vui lòng nhập đủ thông tin!", Snackbar.LENGTH_SHORT).show()
                return@setOnClickListener
            }

            if (checkLogin(username, password)) {
                val speed = getSpeedForUser(username)
                val name = getNamerForUser(username)

                getSharedPreferences("user", MODE_PRIVATE).edit()
                    .putString("username", username)
                    .putFloat("speed", speed)
                    .putString("name", name)
                    .apply()
                saveLoginInfo(username, speed, name)

                Snackbar.make(btnLogin, "✅ Đăng nhập thành công!", Snackbar.LENGTH_SHORT).show()

                startActivity(Intent(this, SsidMenu::class.java))
                overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out)
                finish()
            } else {
                Snackbar.make(btnLogin, "❌ Sai tài khoản hoặc mật khẩu!", Snackbar.LENGTH_SHORT).show()
            }
        }
    }

    // ====== HÀM ĐỌC / GHI FILE login.json ======

    private fun saveLoginInfo(username: String, speed: Float, name: String) {
        val json = JSONObject()
        json.put("username", username)
        json.put("speed", speed)
        json.put("name", name)

        val file = File(filesDir, "login.json")
        file.writeText(json.toString())
    }

    private fun readLoginInfo(): JSONObject? {
        val file = File(filesDir, "login.json")
        if (!file.exists()) return null
        val content = file.readText(Charset.forName("UTF-8"))
        return try {
            JSONObject(content)
        } catch (e: Exception) {
            null
        }
    }

    fun clearLoginInfo() {
        val file = File(filesDir, "login.json")
        if (file.exists()) file.delete()
    }
    private fun checkLogin(username: String, password: String): Boolean {
        val json = loadJSONFromAssets()
        val users = json.getJSONArray("users")
        for (i in 0 until users.length()) {
            val user = users.getJSONObject(i)
            if (user.getString("username") == username && user.getString("password") == password) {
                return true
            }
        }
        return false
    }

    private fun getSpeedForUser(username: String): Float {
        val json = loadJSONFromAssets()
        val users = json.getJSONArray("users")
        for (i in 0 until users.length()) {
            val user = users.getJSONObject(i)
            if (user.getString("username") == username) {
                return user.getDouble("speed").toFloat()
            }
        }
        return 1.0F // mặc định
    }

    private fun getNamerForUser(username: String): String {
        val json = loadJSONFromAssets()
        val users = json.getJSONArray("users")
        for (i in 0 until users.length()) {
            val user = users.getJSONObject(i)
            if (user.getString("username") == username) {
                return user.getString("name")
            }
        }
        return "Người dùng" // mặc định
    }

    private fun loadJSONFromAssets(): JSONObject {
        val inputStream = assets.open("users.json")
        val size = inputStream.available()
        val buffer = ByteArray(size)
        inputStream.read(buffer)
        inputStream.close()
        val jsonString = String(buffer, Charset.forName("UTF-8"))
        return JSONObject(jsonString)
    }
}
