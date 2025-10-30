package com.example.connectesp

import android.content.Intent
import android.os.Bundle
import android.widget.Button
import android.widget.EditText
import com.google.android.material.snackbar.Snackbar
import androidx.appcompat.app.AppCompatActivity
import org.json.JSONObject
import java.nio.charset.Charset

class LoginActivity : AppCompatActivity() {
    private lateinit var edtUsername: EditText
    private lateinit var edtPassword: EditText
    private lateinit var btnLogin: Button

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_login)

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
                getSharedPreferences("user", MODE_PRIVATE).edit()
                    .putString("username", username)
                    .putInt("speed", speed)
                    .apply()

                Snackbar.make(btnLogin, "✅ Đăng nhập thành công!", Snackbar.LENGTH_SHORT).show()

                startActivity(Intent(this, SsidMenu::class.java))
                overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out)
                finish()
            } else {
                Snackbar.make(btnLogin, "❌ Sai tài khoản hoặc mật khẩu!", Snackbar.LENGTH_SHORT).show()
            }
        }
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

    private fun getSpeedForUser(username: String): Int {
        val json = loadJSONFromAssets()
        val users = json.getJSONArray("users")
        for (i in 0 until users.length()) {
            val user = users.getJSONObject(i)
            if (user.getString("username") == username) {
                return user.getInt("speed")
            }
        }
        return 30 // mặc định
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
