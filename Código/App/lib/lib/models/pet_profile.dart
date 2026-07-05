import 'dart:convert';
import 'package:shared_preferences/shared_preferences.dart';

/// Porte do animal, calculado a partir do peso.
enum PetSize { mini, pequeno, medio, grande, gigante }

extension PetSizeLabel on PetSize {
  String get label {
    switch (this) {
      case PetSize.mini:
        return 'Mini';
      case PetSize.pequeno:
        return 'Pequeno';
      case PetSize.medio:
        return 'Médio';
      case PetSize.grande:
        return 'Grande';
      case PetSize.gigante:
        return 'Gigante';
    }
  }
}

/// Linha da tabela peso → porte → gramas sugeridas por refeição.
class PetSizeRow {
  final PetSize size;
  final double minKg;
  final double? maxKg; // null = sem limite superior
  final int suggestedGrams; // por refeição (considerando ~2-3 refeições/dia)

  const PetSizeRow({
    required this.size,
    required this.minKg,
    required this.maxKg,
    required this.suggestedGrams,
  });

  String get weightLabel =>
      maxKg == null ? 'acima de ${minKg.toInt()} kg' : '${minKg.toInt()}–${maxKg!.toInt()} kg';
}

/// Tabela de referência (regra prática: ~20-30g de ração seca por kg de peso
/// corporal ao dia, dividido em 2-3 refeições). Serve como sugestão inicial;
/// o tutor pode ajustar livremente.
class PetSizeTable {
  static const List<PetSizeRow> rows = [
    PetSizeRow(size: PetSize.mini, minKg: 0, maxKg: 3, suggestedGrams: 20),
    PetSizeRow(size: PetSize.pequeno, minKg: 3, maxKg: 10, suggestedGrams: 40),
    PetSizeRow(size: PetSize.medio, minKg: 10, maxKg: 25, suggestedGrams: 80),
    PetSizeRow(size: PetSize.grande, minKg: 25, maxKg: 45, suggestedGrams: 120),
    PetSizeRow(size: PetSize.gigante, minKg: 45, maxKg: null, suggestedGrams: 160),
  ];

  static PetSizeRow rowFor(double weightKg) {
    for (final r in rows) {
      if (weightKg >= r.minKg && (r.maxKg == null || weightKg < r.maxKg!)) {
        return r;
      }
    }
    return rows.last;
  }
}

class PetProfile {
  final double weightKg;
  final int defaultGrams;

  PetProfile({required this.weightKg, required this.defaultGrams});

  PetSize get size => PetSizeTable.rowFor(weightKg).size;

  Map<String, dynamic> toJson() => {
        'weightKg': weightKg,
        'defaultGrams': defaultGrams,
      };

  factory PetProfile.fromJson(Map<String, dynamic> json) => PetProfile(
        weightKg: (json['weightKg'] ?? 10).toDouble(),
        defaultGrams: json['defaultGrams'] ?? 40,
      );

  factory PetProfile.defaultProfile() =>
      PetProfile(weightKg: 10, defaultGrams: 40);
}

class PetProfileService {
  static const _key = 'pet_profile';

  static Future<PetProfile> get() async {
    final prefs = await SharedPreferences.getInstance();
    final raw = prefs.getString(_key);
    if (raw == null) return PetProfile.defaultProfile();
    return PetProfile.fromJson(jsonDecode(raw));
  }

  static Future<void> save(PetProfile profile) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_key, jsonEncode(profile.toJson()));
  }
}
