import 'dart:convert';
import 'package:shared_preferences/shared_preferences.dart';

class Schedule {
  final String id;
  final String type; // 'food'
  final int hour;
  final int minute;
  bool enabled;

  Schedule({
    required this.id,
    required this.type,
    required this.hour,
    required this.minute,
    this.enabled = true,
  });

  String get timeLabel =>
      '${hour.toString().padLeft(2, '0')}:${minute.toString().padLeft(2, '0')}';

  Map<String, dynamic> toJson() => {
        'id': id,
        'type': type,
        'hour': hour,
        'minute': minute,
        'enabled': enabled,
      };

  factory Schedule.fromJson(Map<String, dynamic> json) => Schedule(
        id: json['id'],
        type: json['type'],
        hour: json['hour'],
        minute: json['minute'],
        enabled: json['enabled'] ?? true,
      );
}

class ScheduleService {
  static const _key = 'schedules';

  static Future<List<Schedule>> getAll() async {
    final prefs = await SharedPreferences.getInstance();
    final raw = prefs.getStringList(_key) ?? [];
    final list = raw.map((e) => Schedule.fromJson(jsonDecode(e))).toList();
    list.sort((a, b) {
      final aMin = a.hour * 60 + a.minute;
      final bMin = b.hour * 60 + b.minute;
      return aMin.compareTo(bMin);
    });
    return list;
  }

  static Future<void> save(List<Schedule> schedules) async {
    final prefs = await SharedPreferences.getInstance();
    final raw = schedules.map((e) => jsonEncode(e.toJson())).toList();
    await prefs.setStringList(_key, raw);
  }

  static Future<void> add(Schedule s) async {
    final list = await getAll();
    list.add(s);
    await save(list);
  }

  static Future<void> remove(String id) async {
    final list = await getAll();
    list.removeWhere((s) => s.id == id);
    await save(list);
  }

  static Future<void> toggle(String id, bool enabled) async {
    final list = await getAll();
    final idx = list.indexWhere((s) => s.id == id);
    if (idx >= 0) {
      list[idx].enabled = enabled;
      await save(list);
    }
  }
}
