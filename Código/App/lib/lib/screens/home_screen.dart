import 'dart:async';
import 'package:flutter/material.dart';
import 'package:percent_indicator/circular_percent_indicator.dart';
import '../theme.dart';
import '../services/device_service.dart';
import '../services/data_service.dart';
import '../models/pet_profile.dart';

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen>
    with SingleTickerProviderStateMixin {
  DeviceStatus _status = DeviceStatus.offline();
  bool _loadingFood = false;
  Timer? _timer;
  String _lastFood = '--';
  int _profileGrams = 40;
  late AnimationController _pulseCtrl;
  late Animation<double> _pulse;

  @override
  void initState() {
    super.initState();
    _pulseCtrl = AnimationController(
      vsync: this,
      duration: const Duration(seconds: 2),
    )..repeat(reverse: true);
    _pulse = Tween<double>(begin: 0.95, end: 1.05).animate(
      CurvedAnimation(parent: _pulseCtrl, curve: Curves.easeInOut),
    );
    _fetchStatus();
    _loadLastFood();
    _loadProfile();
    _timer = Timer.periodic(const Duration(seconds: 30), (_) => _fetchStatus());
  }

  @override
  void dispose() {
    _timer?.cancel();
    _pulseCtrl.dispose();
    super.dispose();
  }

  Future<void> _fetchStatus() async {
    final s = await DeviceService.getStatus();
    if (mounted) setState(() => _status = s);
  }

  Future<void> _loadProfile() async {
    final profile = await PetProfileService.get();
    if (mounted) setState(() => _profileGrams = profile.defaultGrams);
  }

  Future<void> _loadLastFood() async {
    final history = await HistoryService.getAll();
    final rec = history.where((r) => r.type == 'food').firstOrNull;
    if (mounted) {
      setState(() => _lastFood = rec != null ? _fmt(rec.timestamp) : 'Nunca');
    }
  }

  String _fmt(DateTime dt) {
    final diff = DateTime.now().difference(dt);
    if (diff.inMinutes < 1) return 'agora mesmo';
    if (diff.inMinutes < 60) return 'há ${diff.inMinutes}min';
    if (diff.inHours < 24) return 'há ${diff.inHours}h';
    return '${dt.day}/${dt.month} às ${dt.hour.toString().padLeft(2, '0')}:${dt.minute.toString().padLeft(2, '0')}';
  }

  Future<void> _dispenseFood() async {
    setState(() => _loadingFood = true);
    final profile = await PetProfileService.get();
    final ok = await DeviceService.dispenseFood(grams: profile.defaultGrams);
    if (ok) {
      await HistoryService.addRecord('food', 'manual');
      await _loadLastFood();
      if (mounted) _showSnack('Ração dispensada! 🐾', AppTheme.green);
    } else {
      if (mounted) _showSnack('Sem conexão com o dispositivo', AppTheme.red);
    }
    if (mounted) setState(() => _loadingFood = false);
  }

  void _showSnack(String msg, Color color) {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(msg,
            style: const TextStyle(
                color: Colors.black, fontWeight: FontWeight.w600)),
        backgroundColor: color,
        behavior: SnackBarBehavior.floating,
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
        margin: const EdgeInsets.all(16),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: AppTheme.bg,
      body: SafeArea(
        child: RefreshIndicator(
          color: AppTheme.green,
          backgroundColor: AppTheme.card,
          onRefresh: _fetchStatus,
          child: SingleChildScrollView(
            physics: const AlwaysScrollableScrollPhysics(),
            padding: const EdgeInsets.symmetric(horizontal: 20),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const SizedBox(height: 24),

                // Header
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        const Text('Feedpet',
                            style: TextStyle(
                              fontSize: 28,
                              fontWeight: FontWeight.w800,
                              color: AppTheme.textPrimary,
                              letterSpacing: -1,
                            )),
                        Text(
                          _status.connected
                              ? 'Dispositivo conectado'
                              : 'Dispositivo offline',
                          style: TextStyle(
                            fontSize: 13,
                            color: _status.connected
                                ? AppTheme.green
                                : AppTheme.red,
                          ),
                        ),
                      ],
                    ),
                    // Status dot
                    AnimatedBuilder(
                      animation: _pulseCtrl,
                      builder: (_, __) => Transform.scale(
                        scale: _status.connected ? _pulse.value : 1.0,
                        child: Container(
                          width: 44,
                          height: 44,
                          decoration: BoxDecoration(
                            color: _status.connected
                                ? AppTheme.greenDim
                                : AppTheme.surface,
                            borderRadius: BorderRadius.circular(12),
                            border: Border.all(
                              color: _status.connected
                                  ? AppTheme.green.withOpacity(0.4)
                                  : AppTheme.border,
                            ),
                          ),
                          child: Icon(
                            _status.connected ? Icons.wifi : Icons.wifi_off,
                            color: _status.connected
                                ? AppTheme.green
                                : AppTheme.textMuted,
                            size: 20,
                          ),
                        ),
                      ),
                    ),
                  ],
                ),

                const SizedBox(height: 28),

                // Níveis
                Row(
                  children: [
                    Expanded(
                      child: _LevelCard(
                        label: 'Ração',
                        level: _status.foodLevel / 100,
                        color: AppTheme.green,
                        dimColor: AppTheme.greenDim,
                        icon: '🍖',
                      ),
                    ),
                    const SizedBox(width: 12),
                    Expanded(
                      child: _LevelCard(
                        label: 'Água',
                        level: _status.waterLevel / 100,
                        color: AppTheme.blue,
                        dimColor: AppTheme.blueDim,
                        icon: '💧',
                      ),
                    ),
                  ],
                ),

                const SizedBox(height: 20),

                // Última alimentação
                Container(
                  padding: const EdgeInsets.all(16),
                  decoration: BoxDecoration(
                    color: AppTheme.surface,
                    borderRadius: BorderRadius.circular(16),
                    border: Border.all(color: AppTheme.border, width: 0.5),
                  ),
                  child: Row(
                    children: [
                      const Text('🕐', style: TextStyle(fontSize: 22)),
                      const SizedBox(width: 12),
                      Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          const Text('Última ração',
                              style: TextStyle(
                                  fontSize: 12, color: AppTheme.textSecondary)),
                          Text(_lastFood,
                              style: const TextStyle(
                                fontSize: 15,
                                fontWeight: FontWeight.w600,
                                color: AppTheme.textPrimary,
                              )),
                        ],
                      ),
                    ],
                  ),
                ),

                const SizedBox(height: 20),

                // Botão principal
                GestureDetector(
                  onTap: _loadingFood ? null : _dispenseFood,
                  child: AnimatedContainer(
                    duration: const Duration(milliseconds: 150),
                    width: double.infinity,
                    padding: const EdgeInsets.symmetric(vertical: 20),
                    decoration: BoxDecoration(
                      color: _loadingFood
                          ? AppTheme.greenDim
                          : AppTheme.green,
                      borderRadius: BorderRadius.circular(18),
                    ),
                    child: Center(
                      child: _loadingFood
                          ? const SizedBox(
                              width: 22,
                              height: 22,
                              child: CircularProgressIndicator(
                                strokeWidth: 2.5,
                                color: Colors.black,
                              ),
                            )
                          : Row(
                              mainAxisSize: MainAxisSize.min,
                              children: [
                                Text('🍖',
                                    style: TextStyle(fontSize: 20)),
                                SizedBox(width: 10),
                                Text(
                                  'Dar ração agora · ${_profileGrams}g',
                                  style: const TextStyle(
                                    fontSize: 16,
                                    fontWeight: FontWeight.w700,
                                    color: Colors.black,
                                    letterSpacing: -0.3,
                                  ),
                                ),
                              ],
                            ),
                    ),
                  ),
                ),

                const SizedBox(height: 32),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class _LevelCard extends StatelessWidget {
  final String label;
  final double level;
  final Color color;
  final Color dimColor;
  final String icon;

  const _LevelCard({
    required this.label,
    required this.level,
    required this.color,
    required this.dimColor,
    required this.icon,
  });

  String get _levelLabel {
    if (level > 0.6) return 'Cheio';
    if (level > 0.3) return 'Médio';
    return 'Baixo';
  }

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(20),
      decoration: BoxDecoration(
        color: AppTheme.card,
        borderRadius: BorderRadius.circular(20),
        border: Border.all(color: AppTheme.border, width: 0.5),
      ),
      child: Column(
        children: [
          CircularPercentIndicator(
            radius: 44,
            lineWidth: 6,
            percent: level.clamp(0.0, 1.0),
            center: Text(icon, style: const TextStyle(fontSize: 24)),
            progressColor: color,
            backgroundColor: color.withOpacity(0.1),
            circularStrokeCap: CircularStrokeCap.round,
          ),
          const SizedBox(height: 12),
          Text(label,
              style: const TextStyle(
                  fontWeight: FontWeight.w600,
                  fontSize: 14,
                  color: AppTheme.textPrimary)),
          const SizedBox(height: 2),
          Text(
            '${(level * 100).toInt()}% · $_levelLabel',
            style: TextStyle(fontSize: 12, color: color),
          ),
        ],
      ),
    );
  }
}
