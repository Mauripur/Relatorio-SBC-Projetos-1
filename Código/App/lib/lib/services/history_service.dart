import 'dart:convert';
import 'package:shared_preferences/shared_preferences.dart';

class FeedingRecord {
  final String id;
  final String type; // 'food' ou 'water'
  final DateTime timestamp;
  final String source; // 'manual' ou 'agendado'

  FeedingRecord({
    required this.id,
    required this.type,
    required this.timestamp,
    required this.source,
  });

  Map<String, dynamic> toJson() => {
        'id': id,
        'type': type,
        'timestamp': timestamp.toIso8601String(),
        'source': source,
      };

  factory FeedingRecord.fromJson(Map<String, dynamic> json) => FeedingRecord(
        id: json['id'],
        type: json['type'],
        timestamp: DateTime.parse(json['timestamp']),
        source: json['source'],
      );
}

class HistoryService {
  static const _key = 'feeding_history';

  static Future<List<FeedingRecord>> getAll() async {
    final prefs = await SharedPreferences.getInstance();
    final raw = prefs.getStringList(_key) ?? [];
    return raw
        .map((e) => FeedingRecord.fromJson(jsonDecode(e)))
        .toList()
      ..sort((a, b) => b.timestamp.compareTo(a.timestamp));
  }

  static Future<void> addRecord(String type, String source) async {
    final prefs = await SharedPreferences.getInstance();
    final raw = prefs.getStringList(_key) ?? [];

    final record = FeedingRecord(
      id: DateTime.now().millisecondsSinceEpoch.toString(),
      type: type,
      timestamp: DateTime.now(),
      source: source,
    );

    raw.insert(0, jsonEncode(record.toJson()));

    // Manter apenas os últimos 200 registros
    if (raw.length > 200) raw.removeRange(200, raw.length);

    await prefs.setStringList(_key, raw);
  }

  static Future<void> clearAll() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.remove(_key);
  }
}
