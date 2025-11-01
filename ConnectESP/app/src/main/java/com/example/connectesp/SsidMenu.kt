package com.example.connectesp

import android.Manifest
import android.content.*
import android.content.pm.PackageManager
import android.net.*
import android.net.wifi.ScanResult
import android.net.wifi.WifiManager
import android.net.wifi.WifiNetworkSpecifier
import android.os.Build
import android.os.Bundle
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.TextView
import androidx.activity.result.contract.ActivityResultContracts
import androidx.annotation.RequiresApi
import androidx.annotation.RequiresPermission
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import okhttp3.*
import java.util.concurrent.TimeUnit
import com.google.android.material.button.MaterialButton
import androidx.core.content.ContextCompat
import android.view.Gravity

class SsidMenu : AppCompatActivity() {

    private lateinit var wifiManager: WifiManager
    private lateinit var llNetworks: LinearLayout
    private lateinit var tvStatus: TextView
    private lateinit var progressScan: ProgressBar
    private lateinit var btnRetryScan: Button

    private var connectivityManager: ConnectivityManager? = null
    private var networkCallback: ConnectivityManager.NetworkCallback? = null
    // Global trong SsidMenu.kt
    private val roadNameMap = mapOf(
        "DBP" to "Điện Biên Phủ",
        "NTP" to "Nguyễn Tri Phương",
        "LQ"  to "Lý Thường Kiệt",
        "HQ"  to "Hàm Nghi"
        // Thêm các tuyến khác nếu cần
    )

    private val client = OkHttpClient.Builder()
        .connectTimeout(6, TimeUnit.SECONDS)
        .readTimeout(6, TimeUnit.SECONDS)
        .build()

    private val espApiUrl = "http://192.168.4.1/api"

    private val PERMISSIONS = arrayOf(
        Manifest.permission.ACCESS_FINE_LOCATION,
        Manifest.permission.ACCESS_WIFI_STATE,
        Manifest.permission.CHANGE_WIFI_STATE,
        Manifest.permission.CHANGE_NETWORK_STATE
    )

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_ssid_menu)

        wifiManager = applicationContext.getSystemService(WIFI_SERVICE) as WifiManager
        llNetworks = findViewById(R.id.llNetworks)
        tvStatus = findViewById(R.id.tvStatus)
        progressScan = findViewById(R.id.progressScan)   // 👈 Lấy ProgressBar

        btnRetryScan = findViewById(R.id.btnRetryScan)
        btnRetryScan.setOnClickListener {
            tvStatus.text = "🔍 Đang quét lại..."
            startScanWithPermissionCheck()
            btnRetryScan.visibility = View.GONE   // Ẩn lại trong lúc quét
        }


        connectivityManager = getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager

        checkAndRequestPermissions()
        registerReceiver(scanReceiver, IntentFilter(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION))
    }

    override fun onResume() {
        super.onResume()
        startScanWithPermissionCheck()   // 👈 Quét tự động khi vào màn hình
    }

    override fun onDestroy() {
        super.onDestroy()
        unregisterReceiver(scanReceiver)
        disconnectFromEsp()
    }

    // 🔌 Ngắt kết nối khỏi Wi-Fi ESP khi thoát app
    private fun disconnectFromEsp() {
        try {
            networkCallback?.let {
                connectivityManager?.unregisterNetworkCallback(it)
                tvStatus.text = "📴 Ngắt kết nối TrafficLight"
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        EspApiClient.activeNetwork = null
    }

    private fun checkAndRequestPermissions() {
        val missing = PERMISSIONS.filter {
            ActivityCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }
        if (missing.isNotEmpty()) {
            ActivityCompat.requestPermissions(this, missing.toTypedArray(), 1)
        }
    }

    // 📡 Nhận kết quả scan
    private val scanReceiver = object : BroadcastReceiver() {
        @RequiresApi(Build.VERSION_CODES.Q)
        @RequiresPermission(Manifest.permission.ACCESS_FINE_LOCATION)
        override fun onReceive(context: Context?, intent: Intent?) {
            progressScan.visibility = View.GONE   // 👈 Ẩn progress khi có kết quả
            val success = intent?.getBooleanExtra(WifiManager.EXTRA_RESULTS_UPDATED, false) ?: false
            if (success) showScanResults() else tvStatus.text = "❌ Quét thất bại"
        }
    }

    private fun startScanWithPermissionCheck() {
        if (!hasPermissions()) {
            requestPermissionsLauncher.launch(PERMISSIONS)
            return
        }
        if (!wifiManager.isWifiEnabled) {
            wifiManager.isWifiEnabled = true
            tvStatus.text = "🔄 Bật Wi-Fi..."
        }

        progressScan.visibility = View.VISIBLE  // 👈 Hiện ProgressBar khi quét
        val started = wifiManager.startScan()
        tvStatus.text = if (started) "🔍 Đang quét Đèn giao thông..." else "⚠️ Không thể bắt đầu quét"
    }

    private fun hasPermissions(): Boolean {
        for (p in PERMISSIONS) {
            if (ActivityCompat.checkSelfPermission(this, p) != PackageManager.PERMISSION_GRANTED) return false
        }
        return true
    }

    private val requestPermissionsLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { perms ->
        val granted = perms.values.all { it }
        if (granted) startScanWithPermissionCheck()
        else tvStatus.text = "⚠️ Cần quyền Location để quét Wi-Fi"
    }

    @RequiresApi(Build.VERSION_CODES.Q)
    @RequiresPermission(Manifest.permission.ACCESS_FINE_LOCATION)
    private fun showScanResults() {
        val results: List<ScanResult> = wifiManager.scanResults
        llNetworks.removeAllViews()

        val espNetworks = results.filter { it.SSID.startsWith("TrafficLight_", ignoreCase = true) }

        if (espNetworks.isEmpty()) {
            tvStatus.text = "❌ Không tìm thấy đèn giao thông nào."
            btnRetryScan.visibility = View.VISIBLE   // 👈 Hiện nút Retry
            return
        }
        else
            btnRetryScan.visibility = View.GONE

        tvStatus.text = "✅ Tìm thấy ${espNetworks.size} đèn giao thông"
        val sorted = espNetworks.sortedByDescending { it.level }

        for (r in sorted) addNetworkButton(r)
    }

    @RequiresApi(Build.VERSION_CODES.Q)
    private fun addNetworkButton(scanResult: ScanResult) {
        val ssid = scanResult.SSID
        if (ssid.isNullOrBlank()) return

        val codes = ssid.substringAfter("TrafficLight_", "Không rõ")
        val codeParts = codes.split("_")
        val displayName = codeParts.joinToString(" - ") { code ->
            roadNameMap[code] ?: code
        }

        val btn = MaterialButton(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            ).apply {
                setMargins(0, 12, 0, 12)  // Khoảng cách giữa các nút
            }

            text = "🚦 $displayName"
            setTextColor(ContextCompat.getColor(context, android.R.color.white))
            textSize = 16f
            gravity = Gravity.CENTER

            // 🎨 Màu nền và bo góc
            setBackgroundColor(ContextCompat.getColor(context, R.color.teal_700))
            cornerRadius = 32   // Bo tròn

            // Hiệu ứng nhấn
            rippleColor = ContextCompat.getColorStateList(context, android.R.color.white)

            // Click
            setOnClickListener { promptConnectToSSID(ssid) }
        }

        llNetworks.addView(btn)
    }


    @RequiresApi(Build.VERSION_CODES.Q)
    private fun promptConnectToSSID(ssid: String) {
        tvStatus.text = "🔗 Đang kết nối tới $ssid..."
        connectUsingNetworkSpecifier(ssid, "12345678")
    }

    // 📶 Kết nối ESP
    @RequiresApi(Build.VERSION_CODES.Q)
    private fun connectUsingNetworkSpecifier(ssid: String, password: String?) {
        val builder = WifiNetworkSpecifier.Builder().setSsid(ssid)
        if (!password.isNullOrEmpty()) builder.setWpa2Passphrase(password)

        val specifier = builder.build()
        val request = NetworkRequest.Builder()
            .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
            .removeCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
            .setNetworkSpecifier(specifier)
            .build()

        networkCallback = object : ConnectivityManager.NetworkCallback() {
            override fun onAvailable(network: Network) {
                runOnUiThread {
                    tvStatus.text = "✅ Đã kết nối tới $ssid"
                }
                EspApiClient.activeNetwork = network
                val intent = Intent(this@SsidMenu, RoadListActivity::class.java)
                startActivity(intent)
            }

            override fun onUnavailable() {
                runOnUiThread { tvStatus.text = "❌ Không thể kết nối tới $ssid" }
            }

            override fun onLost(network: Network) {
                runOnUiThread { tvStatus.text = "⚠️ Mất kết nối $ssid" }
            }
        }

        connectivityManager?.requestNetwork(request, networkCallback!!)
    }
}
