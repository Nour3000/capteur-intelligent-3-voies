clear; clc; close all; rng(11);
set(0, 'DefaultFigurePosition', [100 100 820 500]);
set(0, 'DefaultFigureRenderer', 'painters');
dossier = fullfile(fileparts(mfilename('fullpath')), 'figures');
if ~exist(dossier, 'dir'), mkdir(dossier); end

v = @(T, RH) 331.3 + 0.606*T + 0.0124*RH;
d_min = 2; d_max = 400; d_ref = 100;
T_ref = 20; RH_ref = 50;

precision_module_mm = 3;
precision_LM35_C = 1.5;
Tconv_us = (55.5 + 12.5)/12;
SR_V_us = 0.5;
Vcc_nom = 5; Vcc_min = 4.5; Vcc_max = 5.5;
R1 = 10e3; R2 = 15e3; tol_R = 0.01; alpha_R = 100e-6;
offset_ampli_V = 7e-3; derive_offset_V_C = 7e-6;
Vref = 3.3; Nmax = 4095;
K = R2/(R1 + R2);
V_echo = K*Vcc_nom;
V_seuil = round(V_echo/2*Nmax/Vref)*Vref/Nmax;
T_min = 0; T_max = 40;

fprintf('Grandeurs d''influence (d = %d cm, reference T = %d C, HR = %d %%)\n', d_ref, T_ref, RH_ref);
T = linspace(T_min, T_max, 81);
err_T_sans = d_ref*(v(T_ref, RH_ref)./v(T, RH_ref) - 1);
err_T_avec = d_ref*0.606*precision_LM35_C/v(T_ref, RH_ref);
fprintf('  Temperature : %.3f %%/C ; sans compensation de %.2f a %.2f cm ; avec LM35 (+/- %.1f C) +/- %.2f mm\n', ...
        100*0.606/v(T_ref, RH_ref), min(err_T_sans), max(err_T_sans), precision_LM35_C, 10*err_T_avec);
RH = linspace(0, 100, 51);
err_RH = d_ref*(v(T_ref, RH_ref)./v(T_ref, RH) - 1);
fprintf('  Humidite : de %.2f a %.2f mm entre 0 et 100 %% HR (non compensee)\n', 10*min(err_RH), 10*max(err_RH));
fprintf('  Pression : sans effet sur la vitesse du son d''un gaz parfait a temperature donnee\n');
Vcc = linspace(Vcc_min, Vcc_max, 21);
dt_Vcc = (K*Vcc - 2*V_seuil)/SR_V_us - (K*Vcc_nom - 2*V_seuil)/SR_V_us;
err_Vcc = v(T_ref, RH_ref)*dt_Vcc*1e-6/2*100;
fprintf('  Alimentation %.1f-%.1f V : niveau ECHO %.2f-%.2f V, seuil %.2f V, erreur %.2f a %.2f mm\n', ...
        Vcc_min, Vcc_max, K*Vcc_min, K*Vcc_max, V_seuil, 10*min(err_Vcc), 10*max(err_Vcc));
dK = (R2*(1 + tol_R)/(R1*(1 - tol_R) + R2*(1 + tol_R)) - K)/K;
dK_T = 2*alpha_R*(T_max - T_ref)*R1/(R1 + R2);
dt_K = abs(dK)*V_echo/SR_V_us;
dt_K_T = dK_T*V_echo/SR_V_us;
dt_offset = 2*(offset_ampli_V + derive_offset_V_C*(T_max - T_ref))/SR_V_us;
erreur_CAN_LSB = 2;
dt_CAN = 2*erreur_CAN_LSB*Vref/Nmax/SR_V_us;
fprintf('  Pont diviseur : erreur de gain %.2f %%, derive thermique %.4f %% -> %.3f et %.4f mm\n', ...
        100*dK, 100*dK_T, v(T_ref, RH_ref)*dt_K*1e-6/2*1e3, v(T_ref, RH_ref)*dt_K_T*1e-6/2*1e3);
fprintf('  Decalage LM324 (offset + derive) : %.4f mm ; erreur du seuil CAN (%d LSB) : %.4f mm\n', ...
        v(T_ref, RH_ref)*dt_offset*1e-6/2*1e3, erreur_CAN_LSB, v(T_ref, RH_ref)*dt_CAN*1e-6/2*1e3);

figure(1);
tiledlayout(1, 2, 'TileSpacing', 'compact');
nexttile; plot(T, err_T_sans, 'r', 'LineWidth', 1.8); hold on; grid on;
yline(err_T_avec, 'b--'); yline(-err_T_avec, 'b--'); yline(0, 'k:');
xlabel('T (degC)'); ylabel('erreur a 1 m (cm)');
legend('sans compensation', sprintf('avec LM35 (\\pm%.1f C)', precision_LM35_C), 'Location', 'northeast');
title('Temperature');
nexttile; plot(RH, 10*err_RH, 'm', 'LineWidth', 1.8); grid on; yline(0, 'k:');
xlabel('humidite relative (%)'); ylabel('erreur a 1 m (mm)'); title('Humidite (non compensee)');
exportgraphics(gcf, fullfile(dossier, 'influence_temperature_humidite.png'), 'Resolution', 130);

d = linspace(d_min, d_max, 200);
E_module = precision_module_mm/10./d;
E_temp = 0.606*precision_LM35_C/v(T_ref, RH_ref)*ones(size(d));
E_hum = 0.0124*RH_ref/v(T_ref, RH_ref)*ones(size(d));
E_quant = v(T_ref, RH_ref)*Tconv_us*1e-6/2*100./d;
E_pente = max(abs(err_Vcc))./d;
E_compo = v(T_ref, RH_ref)*(dt_K + dt_K_T + dt_offset + dt_CAN)*1e-6/2*100./d;
E = [E_module; E_temp; E_hum; E_quant; E_pente; E_compo];
noms = {'module HC-SR04', 'temperature (LM35)', 'humidite', 'quantification CAN', 'alimentation', 'composants'};
E_max = sum(abs(E), 1);
E_prob = sqrt(sum(E.^2, 1));
[~, i100] = min(abs(d - d_ref));
fprintf('\nBilan d''incertitude a %d cm (cours : Emax = somme |Ei|, Eprob = racine somme Ei^2)\n', d_ref);
for k = 1:numel(noms)
    fprintf('  %-20s %.4f %%  (%.2f mm)\n', noms{k}, 100*E(k, i100), 10*E(k, i100)*d_ref);
end
fprintf('  Emax = %.3f %% (%.2f mm) ; Eprob = %.3f %% (%.2f mm)\n', 100*E_max(i100), 10*E_max(i100)*d_ref, ...
        100*E_prob(i100), 10*E_prob(i100)*d_ref);

figure(2);
plot(d, 10*E_max.*d, 'k', 'LineWidth', 2); hold on; grid on;
plot(d, 10*E_prob.*d, 'k--', 'LineWidth', 2);
for k = 1:numel(noms), plot(d, 10*E(k, :).*d, 'LineWidth', 1.1); end
xlabel('distance (cm)'); ylabel('incertitude sur d (mm)');
legend(['E_{max}', 'E_{prob}', noms], 'Location', 'northwest');
title('Incertitude en fonction de la distance');
exportgraphics(gcf, fullfile(dossier, 'bilan_incertitude.png'), 'Resolution', 130);

k_vrai = 58.6; t_off_vrai = 120; bruit_us = 20;
d_cal1 = 30; d_cal2 = 200; n_cal = 20;
mesure = @(dist) k_vrai*dist + t_off_vrai + bruit_us*randn(size(dist));
t1 = mean(mesure(d_cal1*ones(1, n_cal)));
t2 = mean(mesure(d_cal2*ones(1, n_cal)));
k_cal = (t2 - t1)/(d_cal2 - d_cal1);
t_off_cal = t2 - k_cal*d_cal2;
d_test = linspace(10, 380, 60);
t_test = mesure(d_test);
d_theo = t_test*v(T_ref, RH_ref)/20000;
d_calib = (t_test - t_off_cal)/k_cal;
fprintf('\nAuto-calibration a deux points (Y = k.X + Yoff), %d et %d cm\n', d_cal1, d_cal2);
fprintf('  k vrai %.2f us/cm, estime %.2f ; Yoff vrai %.0f us, estime %.1f us\n', k_vrai, k_cal, t_off_vrai, t_off_cal);
fprintf('  erreur max : modele theorique %.2f cm, apres calibration %.2f cm\n', ...
        max(abs(d_theo - d_test)), max(abs(d_calib - d_test)));

figure(3);
plot(d_test, d_theo - d_test, 'ro', d_test, d_calib - d_test, 'b*'); grid on; yline(0, 'k:');
xlabel('distance de reference (cm)'); ylabel('erreur (cm)');
legend('modele theorique d = t.v/20000', 'apres calibration 2 points', 'Location', 'northwest');
title(sprintf('Auto-calibration : t = k.d + t_0 (k = %.1f us/cm, t_0 = %d us suppose)', k_vrai, t_off_vrai));
exportgraphics(gcf, fullfile(dossier, 'auto_calibration.png'), 'Resolution', 130);

periode = 0.3; n = 160;
t_s = (0:n-1)*periode;
vraie = 120*ones(1, n);
vraie(61:100) = 120 - 1.5*(0:39);
vraie(101:end) = vraie(100);
brute = vraie + 0.8*randn(1, n);
brute([30 45]) = [40 260];
brute(121:140) = vraie(121:140) + 6*randn(1, 20);
brute(150:156) = NaN;
N_filtre = 8; K_sigma = 3; ecart_min = 5; N_rejet = 3; N_panne = 5; v_max = 300; v_suivi = 50; sigma_instable = 3;
tampon = []; rejets = 0; echecs = 0; derniere = NaN;
filtree = NaN(1, n); defaut = zeros(1, n);
sigma = std(brute(1:20)); ecart_max = max(K_sigma*sigma, ecart_min);
for k = 1:n
    x = brute(k);
    if isnan(x)
        echecs = echecs + 1;
        if echecs >= N_panne, defaut(k) = 4; filtree(k) = NaN; else, filtree(k) = filtree(max(k-1, 1)); end
        continue;
    end
    echecs = 0;
    saut = false; accepte = true;
    if ~isnan(derniere)
        saut = abs(x - derniere)/periode > v_max;
        accepte = ~saut && abs(x - derniere) <= ecart_max + v_suivi*periode;
    end
    if accepte
        tampon = [tampon(max(1, end - N_filtre + 2):end) x]; rejets = 0; derniere = x;
    else
        rejets = rejets + 1; defaut(k) = 1 + saut;
        if rejets >= N_rejet, tampon = x; rejets = 0; derniere = x; end
    end
    if numel(tampon) >= N_filtre && std(diff(tampon))/sqrt(2) > sigma_instable, defaut(k) = 3; end
    filtree(k) = mean(tampon);
end
fprintf('\nDiagnostic sur une sequence simulee de %d mesures\n', n);
fprintf('  rejets : %d ; dont sauts : %d ; cycles instables : %d ; cycles en panne : %d\n', ...
        sum(defaut == 1 | defaut == 2), sum(defaut == 2), sum(defaut == 3), sum(defaut == 4));

figure(4);
plot(t_s, brute, '.', 'Color', [0.6 0.6 0.6], 'MarkerSize', 10); hold on; grid on;
plot(t_s, vraie, 'k:', 'LineWidth', 1.2);
plot(t_s, filtree, 'b', 'LineWidth', 1.8);
marque = {'', 'rx', 'mo', 'gs', 'kv'};
for c = 1:4
    idx = find(defaut == c);
    plot(t_s(idx), min(max(brute(idx), 0), 280), marque{c + 1}, 'MarkerSize', 8, 'LineWidth', 1.5);
end
xlabel('temps (s)'); ylabel('distance (cm)'); ylim([0 300]);
legend('mesure brute', 'distance vraie', 'sortie filtree', 'rejet (ecart trop grand)','saut (> 300 cm/s)', ...
       'instable (bruit > 3 cm)', 'panne (5 echecs)', 'Location', 'northeast');
title('Filtrage et auto-diagnostic (memes regles que le firmware)');
exportgraphics(gcf, fullfile(dossier, 'diagnostic.png'), 'Resolution', 130);

I_mcu_run = 30e-3; I_mcu_veille = 12e-3;
I_hc_actif = 15e-3; I_hc_repos = 2e-3;
I_lcd = 1.5e-3; I_retro = 20e-3;
I_ampli = 0.7e-3; I_lm35 = 60e-6;
I_servo_tenu = 6e-3; I_servo_mouv = 150e-3; t_mouv = 0.3;
t_cycle_actif = 3*(0.006 + 0.060);
mouvements_min = 4;
capacite_mAh = 2000;
moyenne = @(T_per, veille_mcu, coupure_hc, retro, servo_detache) ...
    (t_cycle_actif*I_mcu_run + (T_per - t_cycle_actif)*(veille_mcu*I_mcu_veille + ~veille_mcu*I_mcu_run))/T_per ...
    + 3*(t_cycle_actif/3*I_hc_actif + (~coupure_hc)*(T_per - t_cycle_actif/3)*I_hc_repos)/T_per ...
    + I_lcd + retro*I_retro + I_ampli + I_lm35 ...
    + (~servo_detache)*I_servo_tenu + mouvements_min*t_mouv*I_servo_mouv/60;
scen = {'sans gestion (300 ms)', 'veille + servo coupe (300 ms)', 'mode eco (2 s)', 'eco + capteurs et retroeclairage coupes'};
I_moy = [moyenne(0.3, false, false, true, false), moyenne(0.3, true, false, true, true), ...
         moyenne(2.0, true, false, true, true), moyenne(2.0, true, true, false, true)];
fprintf('\nBilan energetique (alimentation 5 V, batterie %d mAh)\n', capacite_mAh);
for k = 1:numel(scen)
    fprintf('  %-42s %6.1f mA  %5.0f mW  autonomie %5.1f h\n', scen{k}, 1e3*I_moy(k), 5e3*I_moy(k), ...
            capacite_mAh/(1e3*I_moy(k)));
end

figure(5);
b = bar(categorical(scen, scen), 1e3*I_moy, 0.6); grid on;
b.FaceColor = [0.25 0.45 0.7];
text(1:numel(scen), 1e3*I_moy + 2, compose('%.0f mA\n%.1f h', 1e3*I_moy(:), capacite_mAh./(1e3*I_moy(:))), ...
     'HorizontalAlignment', 'center', 'VerticalAlignment', 'bottom');
ylabel('courant moyen (mA)'); ylim([0 1.35e3*max(I_moy)]);
title(sprintf('Strategie energetique : courant moyen et autonomie (%d mAh)', capacite_mAh));
exportgraphics(gcf, fullfile(dossier, 'bilan_energetique.png'), 'Resolution', 130);

R.influence = struct('pct_par_C', 100*0.606/v(T_ref, RH_ref), 'T_sans_min_cm', min(err_T_sans), ...
    'T_sans_max_cm', max(err_T_sans), 'T_avec_mm', 10*err_T_avec, 'RH_min_mm', 10*min(err_RH), ...
    'RH_max_mm', 10*max(err_RH), 'Vcc_min_mm', 10*min(err_Vcc), 'Vcc_max_mm', 10*max(err_Vcc), ...
    'dK_pct', 100*dK, 'dK_T_pct', 100*dK_T, 'V_seuil', V_seuil, 'V_echo', V_echo);
R.incertitude = struct('noms', {noms}, 'E_pct', 100*E(:, i100)', 'E_mm', 10*E(:, i100)'*d_ref, ...
    'Emax_pct', 100*E_max(i100), 'Emax_mm', 10*E_max(i100)*d_ref, 'Eprob_pct', 100*E_prob(i100), ...
    'Eprob_mm', 10*E_prob(i100)*d_ref);
R.calibration = struct('k_vrai', k_vrai, 'k_cal', k_cal, 't_off_vrai', t_off_vrai, 't_off_cal', t_off_cal, ...
    'err_theo_cm', max(abs(d_theo - d_test)), 'err_cal_cm', max(abs(d_calib - d_test)));
R.diagnostic = struct('rejets', sum(defaut == 1 | defaut == 2), 'sauts', sum(defaut == 2), ...
    'instables', sum(defaut == 3), 'pannes', sum(defaut == 4));
R.energie = struct('scenarios', {scen}, 'I_mA', 1e3*I_moy, 'P_mW', 5e3*I_moy, 'autonomie_h', capacite_mAh./(1e3*I_moy));
fid = fopen(fullfile(dossier, 'resultats_capteur_intelligent.json'), 'w');
fwrite(fid, jsonencode(R, 'PrettyPrint', true)); fclose(fid);
