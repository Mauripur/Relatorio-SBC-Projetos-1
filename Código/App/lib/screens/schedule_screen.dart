import 'package:flutter/material.dart';
import '../theme.dart';
import '../services/data_service.dart';
import '../services/device_service.dart';
import '../models/pet_profile.dart';

class ScheduleScreen extends StatefulWidget {
  const ScheduleScreen({super.key});

  @override
  State<ScheduleScreen> createState() => _ScheduleScreenState();
}

class _ScheduleScreenState extends State<ScheduleScreen> {
  List<Schedule> _schedules = [];
  bool _loading = true;

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    final list = await ScheduleService.getAll();
    if (mounted) setState(() { _schedules = list; _loading = false; });
  }

  Future<void> _add() async {
    final picked = await showTimePicker(
      context: context,
      initialTime: TimeOfDay.now(),
      builder: (ctx, child) => Theme(
        data: Theme.of(ctx).copyWith(
          timePickerTheme: TimePickerThemeData(
            backgroundColor: AppTheme.card,
            hourMinuteColor: AppTheme.surface,
            dialBackgroundColor: AppTheme.surface,
            entryModeIconColor: AppTheme.green,
          ),
        ),
        child: child!,
      ),
    );
    if (picked == null) return;
    if (!mounted) return;

    final profile = await PetProfileService.get();
    final result = await showModalBottomSheet<Map<String, dynamic>>(
      context: context,
      backgroundColor: Colors.transparent,
      isScrollControlled: true,
      builder: (_) => _ScheduleDetailsSheet(initialGrams: profile.defaultGrams),
    );
    if (result == null) return;

    final s = Schedule(
      id: DateTime.now().millisecondsSinceEpoch.toString(),
      type: 'food',
      hour: picked.hour,
      minute: picked.minute,
      grams: result['grams'] as int,
      days: List<int>.from(result['days'] as List<int>),
    );
    await ScheduleService.add(s);
    await DeviceService.sendSchedule(s.toJson());
    await _load();

    if (mounted) {
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(
        content: Text('Horário ${s.timeLabel} · ${s.grams}g adicionado!',
            style: const TextStyle(color: Colors.black, fontWeight: FontWeight.w600)),
        backgroundColor: AppTheme.green,
        behavior: SnackBarBehavior.floating,
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
        margin: const EdgeInsets.all(16),
      ));
    }
  }

  Future<void> _delete(Schedule s) async {
    await ScheduleService.remove(s.id);
    await _load();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: AppTheme.bg,
      body: SafeArea(
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Padding(
              padding: const EdgeInsets.fromLTRB(20, 24, 20, 0),
              child: Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  const Text('Horários',
                      style: TextStyle(
                        fontSize: 28,
                        fontWeight: FontWeight.w800,
                        color: AppTheme.textPrimary,
                        letterSpacing: -1,
                      )),
                  GestureDetector(
                    onTap: _add,
                    child: Container(
                      padding: const EdgeInsets.symmetric(
                          horizontal: 14, vertical: 8),
                      decoration: BoxDecoration(
                        color: AppTheme.greenDim,
                        borderRadius: BorderRadius.circular(10),
                        border: Border.all(
                            color: AppTheme.green.withOpacity(0.3)),
                      ),
                      child: const Row(
                        children: [
                          Icon(Icons.add, color: AppTheme.green, size: 16),
                          SizedBox(width: 4),
                          Text('Adicionar',
                              style: TextStyle(
                                  color: AppTheme.green,
                                  fontSize: 13,
                                  fontWeight: FontWeight.w600)),
                        ],
                      ),
                    ),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 8),
            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 20),
              child: Text(
                'Ração dispensada automaticamente nos horários abaixo',
                style: TextStyle(fontSize: 13, color: AppTheme.textSecondary),
              ),
            ),
            const SizedBox(height: 20),
            Expanded(
              child: _loading
                  ? const Center(
                      child: CircularProgressIndicator(color: AppTheme.green))
                  : _schedules.isEmpty
                      ? Center(
                          child: Column(
                            mainAxisAlignment: MainAxisAlignment.center,
                            children: [
                              Text('⏰',
                                  style: TextStyle(
                                      fontSize: 52,
                                      color: AppTheme.textMuted)),
                              const SizedBox(height: 16),
                              const Text('Nenhum horário',
                                  style: TextStyle(
                                      fontSize: 18,
                                      fontWeight: FontWeight.w600,
                                      color: AppTheme.textSecondary)),
                              const SizedBox(height: 6),
                              const Text('Toque em Adicionar para criar',
                                  style: TextStyle(
                                      fontSize: 13,
                                      color: AppTheme.textMuted)),
                            ],
                          ),
                        )
                      : ListView.separated(
                          padding: const EdgeInsets.symmetric(horizontal: 20),
                          itemCount: _schedules.length,
                          separatorBuilder: (_, __) =>
                              const SizedBox(height: 8),
                          itemBuilder: (_, i) {
                            final s = _schedules[i];
                            return Dismissible(
                              key: Key(s.id),
                              direction: DismissDirection.endToStart,
                              background: Container(
                                alignment: Alignment.centerRight,
                                padding: const EdgeInsets.only(right: 20),
                                decoration: BoxDecoration(
                                  color: AppTheme.red.withOpacity(0.2),
                                  borderRadius: BorderRadius.circular(16),
                                ),
                                child: const Icon(Icons.delete_outline,
                                    color: AppTheme.red),
                              ),
                              onDismissed: (_) => _delete(s),
                              child: Container(
                                padding: const EdgeInsets.symmetric(
                                    horizontal: 20, vertical: 16),
                                decoration: BoxDecoration(
                                  color: AppTheme.card,
                                  borderRadius: BorderRadius.circular(16),
                                  border: Border.all(
                                      color: AppTheme.border, width: 0.5),
                                ),
                                child: Row(
                                  children: [
                                    Container(
                                      width: 42,
                                      height: 42,
                                      decoration: BoxDecoration(
                                        color: s.enabled
                                            ? AppTheme.greenDim
                                            : AppTheme.surface,
                                        borderRadius: BorderRadius.circular(10),
                                      ),
                                      child: const Center(
                                          child: Text('🍖',
                                              style:
                                                  TextStyle(fontSize: 20))),
                                    ),
                                    const SizedBox(width: 14),
                                    Expanded(
                                      child: Column(
                                        crossAxisAlignment:
                                            CrossAxisAlignment.start,
                                        children: [
                                          Row(
                                            crossAxisAlignment: CrossAxisAlignment.baseline,
                                            textBaseline: TextBaseline.alphabetic,
                                            children: [
                                              Text(s.timeLabel,
                                                  style: TextStyle(
                                                    fontSize: 24,
                                                    fontWeight: FontWeight.w800,
                                                    letterSpacing: -0.5,
                                                    color: s.enabled
                                                        ? AppTheme.textPrimary
                                                        : AppTheme.textMuted,
                                                  )),
                                              const SizedBox(width: 8),
                                              Text('${s.grams}g',
                                                  style: TextStyle(
                                                    fontSize: 14,
                                                    fontWeight: FontWeight.w700,
                                                    color: s.enabled
                                                        ? AppTheme.green
                                                        : AppTheme.textMuted,
                                                  )),
                                            ],
                                          ),
                                          Text(
                                            s.daysLabel,
                                            style: TextStyle(
                                              fontSize: 12,
                                              color: s.enabled
                                                  ? AppTheme.green
                                                  : AppTheme.textMuted,
                                            ),
                                          ),
                                        ],
                                      ),
                                    ),
                                    Switch(
                                      value: s.enabled,
                                      activeColor: AppTheme.green,
                                      onChanged: (v) async {
                                        await ScheduleService.toggle(s.id, v);
                                        await _load();
                                      },
                                    ),
                                  ],
                                ),
                              ),
                            );
                          },
                        ),
            ),
          ],
        ),
      ),
    );
  }
}

// ─── Sheet: gramas + dias da semana ──────────────────
class _ScheduleDetailsSheet extends StatefulWidget {
  final int initialGrams;
  const _ScheduleDetailsSheet({required this.initialGrams});

  @override
  State<_ScheduleDetailsSheet> createState() => _ScheduleDetailsSheetState();
}

class _ScheduleDetailsSheetState extends State<_ScheduleDetailsSheet> {
  late double _grams;
  final Set<int> _days = {}; // vazio = todos os dias

  @override
  void initState() {
    super.initState();
    _grams = widget.initialGrams.toDouble();
  }

  void _toggleDay(int d) {
    setState(() {
      if (_days.contains(d)) {
        _days.remove(d);
      } else {
        _days.add(d);
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: EdgeInsets.only(
        left: 20,
        right: 20,
        top: 20,
        bottom: MediaQuery.of(context).viewInsets.bottom + 24,
      ),
      child: Container(
        padding: const EdgeInsets.all(20),
        decoration: BoxDecoration(
          color: AppTheme.card,
          borderRadius: BorderRadius.circular(24),
          border: Border.all(color: AppTheme.border, width: 0.5),
        ),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text('Quantidade de ração',
                style: TextStyle(
                    fontSize: 18,
                    fontWeight: FontWeight.w800,
                    color: AppTheme.textPrimary)),
            const SizedBox(height: 4),
            Text('Ajuste conforme o porte do seu pet',
                style: TextStyle(fontSize: 12, color: AppTheme.textSecondary)),
            const SizedBox(height: 12),
            Row(
              children: [
                Text('${_grams.toInt()} g',
                    style: const TextStyle(
                        fontSize: 28,
                        fontWeight: FontWeight.w800,
                        color: AppTheme.green)),
              ],
            ),
            SliderTheme(
              data: SliderTheme.of(context).copyWith(
                activeTrackColor: AppTheme.green,
                inactiveTrackColor: AppTheme.surface,
                thumbColor: AppTheme.green,
                overlayColor: AppTheme.green.withOpacity(0.2),
              ),
              child: Slider(
                value: _grams,
                min: 5,
                max: 300,
                divisions: 59,
                onChanged: (v) => setState(() => _grams = v),
              ),
            ),
            Wrap(
              spacing: 6,
              runSpacing: 6,
              children: PetSizeTable.rows.map((r) {
                return GestureDetector(
                  onTap: () => setState(() => _grams = r.suggestedGrams.toDouble()),
                  child: Container(
                    padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
                    decoration: BoxDecoration(
                      color: AppTheme.surface,
                      borderRadius: BorderRadius.circular(10),
                      border: Border.all(color: AppTheme.border),
                    ),
                    child: Text(
                      '${r.size.label} (${r.weightLabel}) · ${r.suggestedGrams}g',
                      style: const TextStyle(fontSize: 11, color: AppTheme.textSecondary),
                    ),
                  ),
                );
              }).toList(),
            ),
            const SizedBox(height: 20),
            const Text('Repetir nos dias',
                style: TextStyle(
                    fontSize: 18,
                    fontWeight: FontWeight.w800,
                    color: AppTheme.textPrimary)),
            const SizedBox(height: 4),
            Text('Nenhum dia marcado = todos os dias',
                style: TextStyle(fontSize: 12, color: AppTheme.textSecondary)),
            const SizedBox(height: 12),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: weekdayLabels.entries.map((e) {
                final selected = _days.contains(e.key);
                return GestureDetector(
                  onTap: () => _toggleDay(e.key),
                  child: Container(
                    width: 40,
                    height: 40,
                    alignment: Alignment.center,
                    decoration: BoxDecoration(
                      color: selected ? AppTheme.green : AppTheme.surface,
                      borderRadius: BorderRadius.circular(10),
                      border: Border.all(
                        color: selected ? AppTheme.green : AppTheme.border,
                      ),
                    ),
                    child: Text(
                      e.value,
                      style: TextStyle(
                        fontSize: 11,
                        fontWeight: FontWeight.w700,
                        color: selected ? Colors.black : AppTheme.textSecondary,
                      ),
                    ),
                  ),
                );
              }).toList(),
            ),
            const SizedBox(height: 24),
            SizedBox(
              width: double.infinity,
              child: ElevatedButton(
                onPressed: () => Navigator.pop(context, {
                  'grams': _grams.toInt(),
                  'days': _days.toList(),
                }),
                child: const Text('Confirmar horário'),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
