package com.meshtrx.app

import android.annotation.SuppressLint
import android.content.Context
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.os.Bundle
import android.util.Log

class LocationHelper(private val context: Context) {

    private val TAG = "LocationHelper"
    private val locationManager = context.getSystemService(Context.LOCATION_SERVICE) as LocationManager
    private var listening = false
    var onLocationUpdate: ((Double, Double) -> Unit)? = null

    private val listener = object : LocationListener {
        override fun onLocationChanged(loc: Location) {
            ServiceState.myLat.postValue(loc.latitude)
            ServiceState.myLon.postValue(loc.longitude)
            onLocationUpdate?.invoke(loc.latitude, loc.longitude)
            Log.d(TAG, "Location: ${loc.latitude}, ${loc.longitude} (${loc.provider})")
        }
        override fun onProviderEnabled(provider: String) {
            Log.d(TAG, "Provider enabled: $provider")
        }
        override fun onProviderDisabled(provider: String) {
            Log.d(TAG, "Provider disabled: $provider")
        }
        @Deprecated("Deprecated in API")
        override fun onStatusChanged(provider: String?, status: Int, extras: Bundle?) {}
    }

    @SuppressLint("MissingPermission")
    fun start() {
        if (listening) return

        // GPS провайдер
        if (locationManager.isProviderEnabled(LocationManager.GPS_PROVIDER)) {
            locationManager.requestLocationUpdates(
                LocationManager.GPS_PROVIDER, 15_000L, 5f, listener)
            Log.d(TAG, "GPS provider started")
        }

        // Network провайдер (Wi-Fi/Cell) — как fallback
        if (locationManager.isProviderEnabled(LocationManager.NETWORK_PROVIDER)) {
            locationManager.requestLocationUpdates(
                LocationManager.NETWORK_PROVIDER, 15_000L, 5f, listener)
            Log.d(TAG, "Network provider started")
        }

        listening = true

        // Последнее известное
        val lastGps = locationManager.getLastKnownLocation(LocationManager.GPS_PROVIDER)
        val lastNet = locationManager.getLastKnownLocation(LocationManager.NETWORK_PROVIDER)
        val best = when {
            lastGps != null && lastNet != null ->
                if (lastGps.time > lastNet.time) lastGps else lastNet
            lastGps != null -> lastGps
            lastNet != null -> lastNet
            else -> null
        }
        if (best != null) {
            ServiceState.myLat.postValue(best.latitude)
            ServiceState.myLon.postValue(best.longitude)
            Log.d(TAG, "Last known: ${best.latitude}, ${best.longitude}")
        } else {
            Log.d(TAG, "No last known location")
        }
    }

    fun stop() {
        locationManager.removeUpdates(listener)
        listening = false
        Log.d(TAG, "Location updates stopped")
    }
}
