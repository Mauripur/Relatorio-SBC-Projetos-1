import 'dart:convert';
import 'package:http/http.dart' as http;
import 'package:shared_preferences/shared_preferences.dart';

class DeviceStatus {
  final double foodLevel;
  final double waterLevel;
  final bool connected;

  DeviceStatus({
    required this.foodLevel,
    required this.waterLevel,
    required this.connected,
  });

  factory DeviceStatus.offline() =>
      DeviceStatus(foodLevel: 0, waterLevel: 0, connected: false);

  factory DeviceStatus.fromJson(Map<String, dynamic> json) => DeviceStatus(
        foodLevel: (json['food_level'] ?? 0).toDouble(),
        waterLevel: (json['water_level'] ?? 0).toDouble(),
        connected: true,
      );
}

class DeviceService {
  static const _ipKey = 'device_ip';
  static const _portKey = 'device_port';
  static const _timeout = Duration(seconds: 5);

  static Future<String> getIP() async {
    final prefs = await SharedPreferences.getInstance();
    return prefs.getString(_ipKey) ?? '';
  }

  static Future<String> getPort() async {
    final prefs = await SharedPreferences.getInstance();
    return prefs.getString(_portKey) ?? '80';
  }

  static Future<void> saveConfig(String ip, String port) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_ipKey, ip);
    await prefs.setString(_portKey, port);
  }

  static Future<String> _baseUrl() async {
    final ip = await getIP();
    final port = await getPort();
    return 'http://$ip:$port';
  }

  static Future<DeviceStatus> getStatus() async {
    try {
      final base = await _baseUrl();
      if (base == 'http://:80') return DeviceStatus.offline();
      final response =
          await http.get(Uri.parse('$base/status')).timeout(_timeout);
      if (response.statusCode == 200) {
        return DeviceStatus.fromJson(jsonDecode(response.body));
      }
      return DeviceStatus.offline();
    } catch (_) {
      return DeviceStatus.offline();
    }
  }

  /// Libera ração manualmente. [grams] é a quantidade em gramas que o
  /// firmware do ESP32 deve dispensar (ex: convertida em tempo de giro do
  /// motor/rosca sem-fim). Vai como query param ?g= na rota /food.
  static Future<bool> dispenseFood({int grams = 40}) async {
    try {
      final base = await _baseUrl();
      final response = await http
          .get(Uri.parse('$base/food?g=$grams'))
          .timeout(_timeout);
      return response.statusCode == 200;
    } catch (_) {
      return false;
    }
  }

  static Future<bool> testConnection(String ip, String port) async {
    try {
      final response = await http
          .get(Uri.parse('http://$ip:$port/status'))
          .timeout(_timeout);
      return response.statusCode == 200;
    } catch (_) {
      return false;
    }
  }

  static Future<bool> sendSchedule(Map<String, dynamic> schedule) async {
    try {
      final base = await _baseUrl();
      final response = await http
          .post(
            Uri.parse('$base/schedule'),
            headers: {'Content-Type': 'application/json'},
            body: jsonEncode(schedule),
          )
          .timeout(_timeout);
      return response.statusCode == 200;
    } catch (_) {
      return false;
    }
  }
}
