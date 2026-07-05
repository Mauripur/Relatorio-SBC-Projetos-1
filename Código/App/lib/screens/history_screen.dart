import 'package:flutter/material.dart';
import 'package:intl/intl.dart';
import '../theme.dart';
import '../services/data_service.dart';

class HistoryScreen extends StatefulWidget {
  const HistoryScreen({super.key});

  @override
  State<HistoryScreen> createState() => _HistoryScreenState();
}

class _HistoryScreenState extends State<HistoryScreen> {
  List<FeedingRecord> _records = [];
  bool _loading = true;

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    final r = await HistoryService.getAll();
    if (mounted) setState(() { _records = r; _loading = false; });
  }

  String _dayLabel(DateTime dt) {
    final now = DateTime.now();
    if (DateFormat('yyyyMMdd').format(dt) == DateFormat('yyyyMMdd').format(now))
      return 'Hoje';
    final y = now.subtract(const Duration(days: 1));
    if (DateFormat('yyyyMMdd').format(dt) == DateFormat('yyyyMMdd').format(y))
      return 'Ontem';
    return DateFormat('dd/MM/yyyy').format(dt);
  }

  @override
  Widget build(BuildContext context) {
    final Map<String, List<FeedingRecord>> grouped = {};
    for (final r in _records) {
      grouped.putIfAbsent(_dayLabel(r.timestamp), () => []).add(r);
    }
    final days = grouped.keys.toList();

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
                  const Text('Histórico',
                      style: TextStyle(
                        fontSize: 28,
                        fontWeight: FontWeight.w800,
                        color: AppTheme.textPrimary,
                        letterSpacing: -1,
                      )),
                  if (_records.isNotEmpty)
                    GestureDetector(
                      onTap: () async {
                        final ok = await showDialog<bool>(
                          context: context,
                          builder: (_) => AlertDialog(
                            backgroundColor: AppTheme.card,
                            title: const Text('Limpar histórico',
                                style: TextStyle(color: AppTheme.textPrimary)),
                            content: const Text('Remover todos os registros?',
                                style:
                                    TextStyle(color: AppTheme.textSecondary)),
                            actions: [
                              TextButton(
                                  onPressed: () => Navigator.pop(context, false),
                                  child: const Text('Cancelar')),
                              TextButton(
                                  onPressed: () => Navigator.pop(context, true),
                                  child: const Text('Limpar',
                                      style: TextStyle(color: AppTheme.red))),
                            ],
                          ),
                        );
                        if (ok == true) {
                          await HistoryService.clearAll();
                          await _load();
                        }
                      },
                      child: const Icon(Icons.delete_sweep_outlined,
                          color: AppTheme.textMuted, size: 22),
                    ),
                ],
              ),
            ),
            const SizedBox(height: 20),
            Expanded(
              child: _loading
                  ? const Center(
                      child: CircularProgressIndicator(color: AppTheme.green))
                  : _records.isEmpty
                      ? Center(
                          child: Column(
                            mainAxisAlignment: MainAxisAlignment.center,
                            children: [
                              const Text('📋',
                                  style: TextStyle(fontSize: 52)),
                              const SizedBox(height: 16),
                              const Text('Sem registros ainda',
                                  style: TextStyle(
                                      fontSize: 18,
                                      fontWeight: FontWeight.w600,
                                      color: AppTheme.textSecondary)),
                              const SizedBox(height: 6),
                              const Text(
                                  'As alimentações vão aparecer aqui',
                                  style: TextStyle(
                                      fontSize: 13,
                                      color: AppTheme.textMuted)),
                            ],
                          ),
                        )
                      : ListView.builder(
                          padding: const EdgeInsets.symmetric(horizontal: 20),
                          itemCount: days.length,
                          itemBuilder: (_, di) {
                            final day = days[di];
                            final recs = grouped[day]!;
                            return Column(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                Padding(
                                  padding:
                                      const EdgeInsets.symmetric(vertical: 12),
                                  child: Text(day,
                                      style: const TextStyle(
                                        fontSize: 13,
                                        fontWeight: FontWeight.w600,
                                        color: AppTheme.textSecondary,
                                        letterSpacing: 0.2,
                                      )),
                                ),
                                Container(
                                  decoration: BoxDecoration(
                                    color: AppTheme.card,
                                    borderRadius: BorderRadius.circular(16),
                                    border: Border.all(
                                        color: AppTheme.border, width: 0.5),
                                  ),
                                  child: Column(
                                    children: recs.asMap().entries.map((e) {
                                      final rec = e.value;
                                      final isFood = rec.type == 'food';
                                      return Column(
                                        children: [
                                          Padding(
                                            padding: const EdgeInsets.symmetric(
                                                horizontal: 16, vertical: 14),
                                            child: Row(
                                              children: [
                                                Container(
                                                  width: 36,
                                                  height: 36,
                                                  decoration: BoxDecoration(
                                                    color: isFood
                                                        ? AppTheme.greenDim
                                                        : AppTheme.blueDim,
                                                    borderRadius:
                                                        BorderRadius.circular(
                                                            8),
                                                  ),
                                                  child: Center(
                                                    child: Text(
                                                        isFood ? '🍖' : '💧',
                                                        style: const TextStyle(
                                                            fontSize: 16)),
                                                  ),
                                                ),
                                                const SizedBox(width: 12),
                                                Expanded(
                                                  child: Column(
                                                    crossAxisAlignment:
                                                        CrossAxisAlignment
                                                            .start,
                                                    children: [
                                                      Text(
                                                        isFood
                                                            ? 'Ração dispensada'
                                                            : 'Água liberada',
                                                        style: const TextStyle(
                                                          fontSize: 14,
                                                          fontWeight:
                                                              FontWeight.w500,
                                                          color: AppTheme
                                                              .textPrimary,
                                                        ),
                                                      ),
                                                      Text(
                                                        rec.source == 'manual'
                                                            ? 'Manual'
                                                            : 'Agendado',
                                                        style: const TextStyle(
                                                            fontSize: 12,
                                                            color: AppTheme
                                                                .textSecondary),
                                                      ),
                                                    ],
                                                  ),
                                                ),
                                                Text(
                                                  DateFormat('HH:mm')
                                                      .format(rec.timestamp),
                                                  style: const TextStyle(
                                                    fontSize: 16,
                                                    fontWeight: FontWeight.w700,
                                                    color: AppTheme.textPrimary,
                                                    letterSpacing: -0.5,
                                                  ),
                                                ),
                                              ],
                                            ),
                                          ),
                                          if (e.key < recs.length - 1)
                                            const Divider(
                                                height: 1,
                                                color: AppTheme.border,
                                                indent: 64),
                                        ],
                                      );
                                    }).toList(),
                                  ),
                                ),
                                const SizedBox(height: 4),
                              ],
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
