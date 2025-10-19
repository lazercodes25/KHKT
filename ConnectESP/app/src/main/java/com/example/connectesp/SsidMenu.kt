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
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import androidx.activity.result.contract.ActivityResultContracts
import androidx.annotation.RequiresApi
import androidx.annotation.RequiresPermission
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import okhttp3.*
import java.util.concurrent.TimeUnit

class SsidMenu : AppCompatActivity() {

    private lateinit var wifiManager: WifiManager
    private lateinit var llNetworks: LinearLayout
    private lateinit var tvStatus: TextView

    private var connectivityManager: ConnectivityManager? = null
    private var networkCallback: ConnectivityManager.NetworkCallback? = null


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

        connectivityManager = getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager

        findViewById<Button>(R.id.btnScan).setOnClickListener {
            startScanWithPermissionCheck()
        }

        checkAndRequestPermissions()
        registerReceiver(scanReceiver, IntentFilter(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION))
    }

    override fun onDestroy() {
        super.onDestroy()
        unregisterReceiver(scanReceiver)
        disconnectFromEsp()
    }

    /**
     * 🔌 Ngắt kết nối khỏi Wi-Fi ESP khi thoát app
     */
    private fun disconnectFromEsp() {
        try {
            networkCallback?.let {
                connectivityManager?.unregisterNetworkCallback(it)
                tvStatus.text = "📴 Ngắt kết nối ESP"
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

    // Receiver để nhận kết quả scan
    private val scanReceiver = object : BroadcastReceiver() {
        @RequiresApi(Build.VERSION_CODES.Q)
        @RequiresPermission(Manifest.permission.ACCESS_FINE_LOCATION)
        override fun onReceive(context: Context?, intent: Intent?) {
            val success = intent?.getBooleanExtra(WifiManager.EXTRA_RESULTS_UPDATED, false) ?: false
            if (success) showScanResults() else tvStatus.text = "Quét thất bại"
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
        val started = wifiManager.startScan()
        tvStatus.text = if (started) "🔍 Đang quét..." else "⚠️ Không thể bắt đầu quét"
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
        if (ActivityCompat.checkSelfPermission(
                this,
                Manifest.permission.ACCESS_FINE_LOCATION
            ) != PackageManager.PERMISSION_GRANTED
        ) return

        val results: List<ScanResult> = wifiManager.scanResults
        llNetworks.removeAllViews()

        // ✅ Lọc chỉ các mạng có SSID chứa "ESP" (hoặc bắt đầu bằng "ESP")
        val espNetworks = results.filter { it.SSID.startsWith("ESP", ignoreCase = true) }

        if (espNetworks.isEmpty()) {
            tvStatus.text = "❌ Không tìm thấy mạng ESP nào."
            return
        }

        tvStatus.text = "✅ Tìm thấy ${espNetworks.size} mạng ESP"

        val sorted = espNetworks.sortedByDescending { it.level }
        for (r in sorted) {
            addNetworkButton(r)
        }
    }

    @RequiresApi(Build.VERSION_CODES.Q)
    private fun addNetworkButton(scanResult: ScanResult) {
        val ssid = scanResult.SSID
        if (ssid.isNullOrBlank()) return

        val btn = Button(this)
        btn.layoutParams = LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT
        )
        btn.text = "$ssid (RSSI: ${scanResult.level})"
        btn.setOnClickListener {
            promptConnectToSSID(ssid)
        }
        llNetworks.addView(btn)
    }

    @RequiresApi(Build.VERSION_CODES.Q)
    private fun promptConnectToSSID(ssid: String) {
        tvStatus.text = "🔗 Đang kết nối tới $ssid..."
        connectUsingNetworkSpecifier(ssid, "12345678")
    }

    // === Android 10+ (ephemeral Wi-Fi) ===
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
                runOnUiThread { tvStatus.text = "✅ Đã kết nối tới $ssid" }
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
