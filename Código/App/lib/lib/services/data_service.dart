import 'dart:convert';
import 'package:shared_preferences/shared_preferences.dart';

// ─── Histórico ───────────────────────────────────────
class FeedingRecord {
  final String id;
  final String type;
  final DateTime timestamp;
  final String source;

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
  static const _key = 'feeding_history_v2';

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
    if (raw.length > 200) raw.removeRange(200, raw.length);
    await prefs.setStringList(_key, raw);
  }

  static Future<void> clearAll() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.remove(_key);
  }
}

// ─── Agendamentos ────────────────────────────────────
/// Dias da semana no padrão DateTime.weekday: 1=Seg ... 7=Dom
const weekdayLabels = {
  1: 'Seg',
  2: 'Ter',
  3: 'Qua',
  4: 'Qui',
  5: 'Sex',
  6: 'Sáb',
  7: 'Dom',
};

class Schedule {
  final String id;
  final String type;
  final int hour;
  final int minute;
  bool enabled;
  int grams; // quantidade de ração a liberar, em gramas
  List<int> days; // 1=Seg ... 7=Dom. Vazio = todos os dias.

  Schedule({
    required this.id,
    required this.type,
    required this.hour,
    required this.minute,
    this.enabled = true,
    this.grams = 40,
    List<int>? days,
  }) : days = days ?? [];

  String get timeLabel =>
      '${hour.toString().padLeft(2, '0')}:${minute.toString().padLeft(2, '0')}';

  String get daysLabel {
    if (days.isEmpty || days.length == 7) return 'Todos os dias';
    final sorted = [...days]..sort();
    return sorted.map((d) => weekdayLabels[d]).join(', ');
  }

  Map<String, dynamic> toJson() => {
        'id': id,
        'type': type,
        'hour': hour,
        'minute': minute,
        'enabled': enabled,
        'grams': grams,
        'days': days,
      };

  factory Schedule.fromJson(Map<String, dynamic> json) => Schedule(
        id: json['id'],
        type: json['type'],
        hour: json['hour'],
        minute: json['minute'],
        enabled: json['enabled'] ?? true,
        grams: json['grams'] ?? 40,
        days: json['days'] != null ? List<int>.from(json['days']) : [],
      );
}

class ScheduleService {
  static const _key = 'schedules_v2';

  static Future<List<Schedule>> getAll() async {
    final prefs = await SharedPreferences.getInstance();
    final raw = prefs.getStringList(_key) ?? [];
    final list = raw.map((e) => Schedule.fromJson(jsonDecode(e))).toList();
    list.sort((a, b) => (a.hour * 60 + a.minute) - (b.hour * 60 + b.minute));
    return list;
  }

  static Future<void> _save(List<Schedule> list) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setStringList(
        _key, list.map((e) => jsonEncode(e.toJson())).toList());
  }

  static Future<void> add(Schedule s) async {
    final list = await getAll();
    list.add(s);
    await _save(list);
  }

  static Future<void> remove(String id) async {
    final list = await getAll();
    list.removeWhere((s) => s.id == id);
    await _save(list);
  }

  static Future<void> toggle(String id, bool enabled) async {
    final list = await getAll();
    final idx = list.indexWhere((s) => s.id == id);
    if (idx >= 0) {
      list[idx].enabled = enabled;
      await _save(list);
    }
  }
}
