import 'package:flutter/material.dart';
import 'package:mobile_scanner/mobile_scanner.dart';
import '../theme.dart';
import '../services/device_service.dart';
import '../models/pet_profile.dart';

class SettingsScreen extends StatefulWidget {
  const SettingsScreen({super.key});

  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  final _ipCtrl = TextEditingController();
  final _portCtrl = TextEditingController();
  bool _testing = false;
  bool? _connected;

  double _weightKg = 10;
  int _grams = 40;
  bool _gramsManuallyEdited = false;

  @override
  void initState() {
    super.initState();
    _load();
    _loadProfile();
  }

  Future<void> _loadProfile() async {
    final profile = await PetProfileService.get();
    if (mounted) {
      setState(() {
        _weightKg = profile.weightKg;
        _grams = profile.defaultGrams;
      });
    }
  }

  Future<void> _saveProfile() async {
    await PetProfileService.save(
      PetProfile(weightKg: _weightKg, defaultGrams: _grams),
    );
  }

  @override
  void dispose() {
    _ipCtrl.dispose();
    _portCtrl.dispose();
    super.dispose();
  }

  Future<void> _load() async {
    _ipCtrl.text = await DeviceService.getIP();
    _portCtrl.text = await DeviceService.getPort();
  }

  Future<void> _save() async {
    await DeviceService.saveConfig(_ipCtrl.text.trim(), _portCtrl.text.trim());
  }

  Future<void> _test() async {
    await _save();
    setState(() { _testing = true; _connected = null; });
    final ok = await DeviceService.testConnection(
        _ipCtrl.text.trim(), _portCtrl.text.trim());
    if (mounted) setState(() { _testing = false; _connected = ok; });
  }

  Future<void> _scanQR() async {
    await Navigator.of(context).push(MaterialPageRoute(
      builder: (_) => _QRScannerScreen(
        onScanned: (value) async {
          // Espera formato: feedpet://192.168.1.100:80
          // (petfeeder:// continua aceito por compatibilidade)
          // ou apenas o IP: 192.168.1.100
          String ip = value;
          String port = '80';

          if (value.startsWith('feedpet://') || value.startsWith('petfeeder://')) {
            final scheme = value.startsWith('feedpet://') ? 'feedpet://' : 'petfeeder://';
            final parts = value.replaceFirst(scheme, '').split(':');
            ip = parts[0];
            if (parts.length > 1) port = parts[1];
          } else if (value.contains(':')) {
            final parts = value.split(':');
            ip = parts[0];
            port = parts[1];
          }

          _ipCtrl.text = ip;
          _portCtrl.text = port;
          await _save();
          if (mounted) {
            ScaffoldMessenger.of(context).showSnackBar(SnackBar(
              content: Text('Conectando a $ip:$port...',
                  style: const TextStyle(
                      color: Colors.black, fontWeight: FontWeight.w600)),
              backgroundColor: AppTheme.green,
              behavior: SnackBarBehavior.floating,
              shape: RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(12)),
              margin: const EdgeInsets.all(16),
            ));
            setState(() {});
            _test();
          }
        },
      ),
    ));
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: AppTheme.bg,
      body: SafeArea(
        child: SingleChildScrollView(
          padding: const EdgeInsets.symmetric(horizontal: 20),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const SizedBox(height: 24),
              const Text('Configurações',
                  style: TextStyle(
                    fontSize: 28,
                    fontWeight: FontWeight.w800,
                    color: AppTheme.textPrimary,
                    letterSpacing: -1,
                  )),
              const SizedBox(height: 4),
              const Text('Conexão com o ESP32',
                  style:
                      TextStyle(fontSize: 13, color: AppTheme.textSecondary)),

              const SizedBox(height: 24),

              // ─── Perfil do Pet ───
              Container(
                width: double.infinity,
                padding: const EdgeInsets.all(20),
                decoration: BoxDecoration(
                  color: AppTheme.card,
                  borderRadius: BorderRadius.circular(18),
                  border: Border.all(color: AppTheme.border, width: 0.5),
                ),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const Row(
                      children: [
                        Text('⚖️', style: TextStyle(fontSize: 18)),
                        SizedBox(width: 8),
                        Text('Perfil do pet',
                            style: TextStyle(
                                fontSize: 15,
                                fontWeight: FontWeight.w700,
                                color: AppTheme.textPrimary)),
                      ],
                    ),
                    const SizedBox(height: 4),
                    Text(
                      'Usado para sugerir a quantidade de ração por refeição',
                      style: TextStyle(fontSize: 12, color: AppTheme.textSecondary),
                    ),
                    const SizedBox(height: 16),
                    Row(
                      mainAxisAlignment: MainAxisAlignment.spaceBetween,
                      children: [
                        Text('Peso do pet',
                            style: TextStyle(fontSize: 13, color: AppTheme.textSecondary)),
                        Text('${_weightKg.toStringAsFixed(1)} kg',
                            style: const TextStyle(
                                fontSize: 15,
                                fontWeight: FontWeight.w700,
                                color: AppTheme.textPrimary)),
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
                        value: _weightKg,
                        min: 0.5,
                        max: 70,
                        divisions: 139,
                        onChanged: (v) {
                          setState(() {
                            _weightKg = v;
                            if (!_gramsManuallyEdited) {
                              _grams = PetSizeTable.rowFor(v).suggestedGrams;
                            }
                          });
                          _saveProfile();
                        },
                      ),
                    ),
                    Row(
                      children: [
                        Container(
                          padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 4),
                          decoration: BoxDecoration(
                            color: AppTheme.greenDim,
                            borderRadius: BorderRadius.circular(8),
                          ),
                          child: Text(
                            'Porte: ${PetSizeTable.rowFor(_weightKg).size.label}',
                            style: const TextStyle(
                                fontSize: 12,
                                fontWeight: FontWeight.w600,
                                color: AppTheme.green),
                          ),
                        ),
                      ],
                    ),
                    const SizedBox(height: 16),
                    Row(
                      mainAxisAlignment: MainAxisAlignment.spaceBetween,
                      children: [
                        Text('Ração por refeição',
                            style: TextStyle(fontSize: 13, color: AppTheme.textSecondary)),
                        Text('$_grams g',
                            style: const TextStyle(
                                fontSize: 15,
                                fontWeight: FontWeight.w700,
                                color: AppTheme.textPrimary)),
                      ],
                    ),
                    SliderTheme(
                      data: SliderTheme.of(context).copyWith(
                        activeTrackColor: AppTheme.blue,
                        inactiveTrackColor: AppTheme.surface,
                        thumbColor: AppTheme.blue,
                        overlayColor: AppTheme.blue.withOpacity(0.2),
                      ),
                      child: Slider(
                        value: _grams.toDouble(),
                        min: 5,
                        max: 300,
                        divisions: 59,
                        onChanged: (v) {
                          setState(() {
                            _grams = v.toInt();
                            _gramsManuallyEdited = true;
                          });
                          _saveProfile();
                        },
                      ),
                    ),
                    Text(
                      'Sugestão baseada no peso (editável). Regra prática: ~20-30g de ração seca por kg/dia, divididos entre as refeições.',
                      style: TextStyle(fontSize: 11, color: AppTheme.textMuted),
                    ),
                  ],
                ),
              ),

              const SizedBox(height: 20),

              // Botão QR code
              GestureDetector(
                onTap: _scanQR,
                child: Container(
                  width: double.infinity,
                  padding: const EdgeInsets.all(20),
                  decoration: BoxDecoration(
                    color: AppTheme.greenDim,
                    borderRadius: BorderRadius.circular(18),
                    border: Border.all(
                        color: AppTheme.green.withOpacity(0.3), width: 1),
                  ),
                  child: const Row(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      Icon(Icons.qr_code_scanner,
                          color: AppTheme.green, size: 26),
                      SizedBox(width: 12),
                      Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text('Escanear QR code do ESP32',
                              style: TextStyle(
                                color: AppTheme.green,
                                fontSize: 15,
                                fontWeight: FontWeight.w700,
                              )),
                          Text('Conectar automaticamente',
                              style: TextStyle(
                                  color: AppTheme.green,
                                  fontSize: 12,
                                  fontWeight: FontWeight.w400)),
                        ],
                      ),
                    ],
                  ),
                ),
              ),

              const SizedBox(height: 20),

              // Divisor
              Row(
                children: [
                  const Expanded(
                      child: Divider(color: AppTheme.border, thickness: 0.5)),
                  Padding(
                    padding: const EdgeInsets.symmetric(horizontal: 12),
                    child: Text('ou insira manualmente',
                        style: TextStyle(
                            fontSize: 12, color: AppTheme.textMuted)),
                  ),
                  const Expanded(
                      child: Divider(color: AppTheme.border, thickness: 0.5)),
                ],
              ),

              const SizedBox(height: 20),

              // Campos manuais
              Container(
                padding: const EdgeInsets.all(20),
                decoration: BoxDecoration(
                  color: AppTheme.card,
                  borderRadius: BorderRadius.circular(18),
                  border: Border.all(color: AppTheme.border, width: 0.5),
                ),
                child: Column(
                  children: [
                    TextField(
                      controller: _ipCtrl,
                      style: const TextStyle(color: AppTheme.textPrimary),
                      decoration: const InputDecoration(
                        labelText: 'Endereço IP do ESP32',
                        hintText: '192.168.1.100',
                        prefixIcon:
                            Icon(Icons.router_outlined, color: AppTheme.green),
                      ),
                      keyboardType: TextInputType.number,
                    ),
                    const SizedBox(height: 12),
                    TextField(
                      controller: _portCtrl,
                      style: const TextStyle(color: AppTheme.textPrimary),
                      decoration: const InputDecoration(
                        labelText: 'Porta',
                        hintText: '80',
                        prefixIcon: Icon(Icons.settings_ethernet,
                            color: AppTheme.green),
                      ),
                      keyboardType: TextInputType.number,
                    ),
                    const SizedBox(height: 16),

                    // Status
                    if (_connected != null)
                      AnimatedContainer(
                        duration: const Duration(milliseconds: 300),
                        width: double.infinity,
                        padding: const EdgeInsets.all(12),
                        margin: const EdgeInsets.only(bottom: 12),
                        decoration: BoxDecoration(
                          color: _connected!
                              ? AppTheme.greenDim
                              : AppTheme.red.withOpacity(0.15),
                          borderRadius: BorderRadius.circular(10),
                        ),
                        child: Row(
                          children: [
                            Icon(
                              _connected!
                                  ? Icons.check_circle_outline
                                  : Icons.error_outline,
                              color:
                                  _connected! ? AppTheme.green : AppTheme.red,
                              size: 18,
                            ),
                            const SizedBox(width: 8),
                            Expanded(
                              child: Text(
                                _connected!
                                    ? 'Conectado com sucesso!'
                                    : 'Não foi possível conectar. Verifique o IP.',
                                style: TextStyle(
                                  fontSize: 13,
                                  color: _connected!
                                      ? AppTheme.green
                                      : AppTheme.red,
                                ),
                              ),
                            ),
                          ],
                        ),
                      ),

                    Row(
                      children: [
                        Expanded(
                          child: OutlinedButton.icon(
                            onPressed: _testing ? null : _test,
                            icon: _testing
                                ? const SizedBox(
                                    width: 14,
                                    height: 14,
                                    child: CircularProgressIndicator(
                                        strokeWidth: 2,
                                        color: AppTheme.green))
                                : const Icon(Icons.wifi_find,
                                    color: AppTheme.green),
                            label: Text(
                                _testing ? 'Testando...' : 'Testar',
                                style:
                                    const TextStyle(color: AppTheme.green)),
                            style: OutlinedButton.styleFrom(
                              side: const BorderSide(color: AppTheme.border),
                              shape: RoundedRectangleBorder(
                                  borderRadius: BorderRadius.circular(12)),
                            ),
                          ),
                        ),
                        const SizedBox(width: 10),
                        Expanded(
                          child: ElevatedButton.icon(
                            onPressed: _save,
                            icon: const Icon(Icons.save_outlined, size: 18),
                            label: const Text('Salvar'),
                          ),
                        ),
                      ],
                    ),
                  ],
                ),
              ),

              const SizedBox(height: 20),

              // Dica QR no ESP32
              Container(
                padding: const EdgeInsets.all(16),
                decoration: BoxDecoration(
                  color: AppTheme.surface,
                  borderRadius: BorderRadius.circular(14),
                  border: Border.all(color: AppTheme.border, width: 0.5),
                ),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const Row(
                      children: [
                        Icon(Icons.lightbulb_outline,
                            color: AppTheme.orange, size: 16),
                        SizedBox(width: 6),
                        Text('Como usar o QR code',
                            style: TextStyle(
                                fontSize: 13,
                                fontWeight: FontWeight.w600,
                                color: AppTheme.textPrimary)),
                      ],
                    ),
                    const SizedBox(height: 10),
                    _tip('No código do ESP32, acesse: http://[IP]/qr no navegador'),
                    _tip('O ESP32 mostra um QR code com o IP dele'),
                    _tip('Escaneie pelo app e ele conecta automaticamente'),
                    _tip('O celular e o ESP32 precisam estar na mesma rede WiFi'),
                  ],
                ),
              ),

              const SizedBox(height: 32),
            ],
          ),
        ),
      ),
    );
  }

  Widget _tip(String text) => Padding(
        padding: const EdgeInsets.only(bottom: 6),
        child: Row(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text('·',
                style:
                    TextStyle(color: AppTheme.textMuted, fontSize: 14)),
            const SizedBox(width: 6),
            Expanded(
                child: Text(text,
                    style: const TextStyle(
                        fontSize: 12, color: AppTheme.textSecondary))),
          ],
        ),
      );
}

// ─── Tela de scanner QR ──────────────────────────────
class _QRScannerScreen extends StatefulWidget {
  final Function(String) onScanned;
  const _QRScannerScreen({required this.onScanned});

  @override
  State<_QRScannerScreen> createState() => _QRScannerScreenState();
}

class _QRScannerScreenState extends State<_QRScannerScreen> {
  bool _scanned = false;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.black,
      appBar: AppBar(
        backgroundColor: Colors.black,
        title: const Text('Escanear QR do ESP32'),
        leading: IconButton(
          icon: const Icon(Icons.close),
          onPressed: () => Navigator.pop(context),
        ),
      ),
      body: Stack(
        children: [
          MobileScanner(
            onDetect: (capture) {
              if (_scanned) return;
              final barcode = capture.barcodes.firstOrNull;
              if (barcode?.rawValue != null) {
                _scanned = true;
                Navigator.pop(context);
                widget.onScanned(barcode!.rawValue!);
              }
            },
          ),
          // Guia visual
          Center(
            child: Container(
              width: 240,
              height: 240,
              decoration: BoxDecoration(
                border: Border.all(color: AppTheme.green, width: 2),
                borderRadius: BorderRadius.circular(16),
              ),
            ),
          ),
          Positioned(
            bottom: 60,
            left: 0,
            right: 0,
            child: Center(
              child: Container(
                padding:
                    const EdgeInsets.symmetric(horizontal: 20, vertical: 10),
                decoration: BoxDecoration(
                  color: Colors.black.withOpacity(0.6),
                  borderRadius: BorderRadius.circular(20),
                ),
                child: const Text(
                  'Aponte para o QR code do ESP32',
                  style: TextStyle(color: Colors.white, fontSize: 14),
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}
