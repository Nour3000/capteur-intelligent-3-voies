clear; clc; close all; rng(7);
set(0, 'DefaultFigurePosition', [100 100 780 480]);

Vcc  = 5;
R1   = 10e3;  R2 = 20e3;
N    = 12;    Nmax = 2^N - 1;
Vref_pcb = 3.3;
Vref_sim = 5.0;
GBW  = 1e6;
SR   = 0.5e6;
Cp   = 20e-12;
Tconv = (55.5+12.5)/12e6;
kModele = 58;
SEUIL = 1000;
v = @(T) 331.3 + 0.606*T;

Vini  = (0:0.5:5)';
R1m = 10.06e3;  R2m = 19.88e3;
Voff = 2e-3;
Voutmi = round((Vini*R2m/(R1m+R2m) + Voff)*1e3)/1e3;
Ncode  = min(round(Voutmi/Vref_pcb*Nmax), Nmax);
Vcalth = Vcc*R2/(R1+R2);
Gn     = Vcalth/Vcc;
Vthi   = Gn*Vini;
Eigm  = (Voutmi - Vthi)/Vcalth;
Egmax = max(abs(Eigm))*100;
p = polyfit(Vini, Voutmi, 1);   a1 = p(1);   b1 = p(2);
Voutmodi = polyval(p, Vini);
Eli   = (Voutmi - Voutmodi)/Vcalth*100;
ELmax = max(abs(Eli));
Eoff  = b1/Vcalth*100;
Sgi   = diff(Voutmi)./diff(Vini);
Sgmoy = sum(Sgi)/(length(Voutmi)-1);
fprintf('\n=== PARTIE 1 : conditionneur (pont diviseur + suiveur) ===\n');
fprintf('Gn theorique = %.4f | a = %.4f | b = %.2f mV | Sgmoy = %.4f V/V\n', Gn, a1, b1*1e3, Sgmoy);
fprintf('Egmax = %.2f %% | ELmax = %.3f %% | Eoff = %.3f %%\n', Egmax, ELmax, Eoff);

figure(1); plot(Vini, Voutmi, 'r*', Vini, Voutmodi, 'b-'); grid on;
xlabel('V_{ECHO} (V)'); ylabel('V_{PA0} (V)'); legend('mesure','modele a V_{in} + b','Location','northwest');
title({'Caracteristique statique du conditionneur', ...
       sprintf('a = %.4f ; b = %.1f mV ; ELmax = %.2f %% ; Eoff = %.2f %%', a1, b1*1e3, ELmax, Eoff)});
figure(2); plot(Voutmi, Eigm*100, 'b*-'); grid on; yline(0,'k:');
xlabel('V_{PA0} (V)'); ylabel('Eigm (%)'); title(sprintf('Erreur de gain : Egmax = %.2f %%', Egmax));
figure(3); plot(Voutmi(1:end-1), Sgi, 'r*-'); grid on; yline(Gn,'k--','Gn theorique');
xlabel('V_{PA0} (V)'); ylabel('Sgi (V/V)'); title(sprintf('Sensibilite : Sgmoy = %.4f V/V', Sgmoy));

Ie  = [0 10 20 25 30 40 50 60 80 100]';
cal_T = 100;
VLM = 10e-3*Ie;
Nc  = round(VLM/Vref_sim*Nmax);
Im_av = Nc*(3300/Nmax)/10;
Im_ap = Nc*(5000/Nmax)/10;
dT_av = Im_av - Ie;   dT_ap = Im_ap - Ie;
ok = Ie > 0;
er_av = dT_av(ok)./Im_av(ok)*100;
Eps_av = dT_av/cal_T*100;   Eps_ap = dT_ap/cal_T*100;
Cl_av = max(abs(Eps_av));   Cl_ap = max(abs(Eps_ap));
Scap = 10e-3;
SCAN_pcb = Nmax/Vref_pcb;   SCAN_sim = Nmax/Vref_sim;
Sg_pcb = Scap*SCAN_pcb;     Sg_sim = Scap*SCAN_sim;
pT = polyfit(Ie, Nc, 1);
fprintf('\n=== PARTIE 2 : chaine de temperature ===\n');
fprintf('Sg = %.2f codes/degC (Vref 3,3 V) | %.2f codes/degC (Vref 5 V) | pente mesuree = %.2f\n', Sg_pcb, Sg_sim, pT(1));
fprintf('Avant correction : erreur de gain %.1f %% | Cl = %.1f %%\n', mean(dT_av(ok)./Ie(ok))*100, Cl_av);
fprintf('Apres correction : dTmax = %.3f degC | Cl = %.3f %%\n', max(abs(dT_ap)), Cl_ap);

figure(4); plot(Ie, Im_av, 'r*-', Ie, Im_ap, 'b*-', Ie, Ie, 'k--'); grid on;
xlabel('Ie : temperature etalon (degC)'); ylabel('Im : indication (degC)');
legend('Vref = 3,3 V dans le code','Vref = 5 V (corrige)','droite ideale','Location','northwest');
title({'Courbe d''etalonnage de la chaine de temperature', ...
       sprintf('Cl avant correction = %.1f %% ; apres = %.2f %%', Cl_av, Cl_ap)});

Ied = [10 20 50 100 150 200 300 400]';
cal_D = 400;
T0  = 25;
jit = Tconv*1e6*(rand(size(Ied)) - rand(size(Ied)));
kT  = 20000/v(T0);
ImA = (Ied*kModele + jit)*v(T0)/20000;
ImB = (Ied*kT      + jit)*v(T0)/20000;
dA = ImA - Ied;  dB = ImB - Ied;
erA = dA./ImA*100;  erB = dB./ImB*100;
ClA = max(abs(dA/cal_D*100));  ClB = max(abs(dB/cal_D*100));
SmoyA = sum(diff(ImA)./diff(Ied))/(length(Ied)-1);
fprintf('\n=== PARTIE 3 : chaine de distance (T = %d degC) ===\n', T0);
fprintf('A (58 us/cm) : Smoy = %.4f | dmax = %.2f cm | Cl = %.2f %%\n', SmoyA, max(abs(dA)), ClA);
fprintf('B (%.2f us/cm) : dmax = %.3f cm | Cl = %.3f %%\n', kT, max(abs(dB)), ClB);

Tl = [0 10 20 30 40]';  d0 = 100;
jit2 = Tconv*1e6*(rand(size(Tl)) - rand(size(Tl)));
kTl  = 20000./v(Tl);
tt   = d0*kTl + jit2;
d_comp = tt.*v(Tl)/20000;
d_fixe = tt.*v(20)/20000;
fprintf('Compensation (d = %d cm) : erreur max sans = %.2f cm | avec = %.3f cm\n', d0, ...
        max(abs(d_fixe-d0)), max(abs(d_comp-d0)));

figure(5); tiledlayout(1,2,'TileSpacing','compact');
nexttile; plot(Ied, ImA, 'r*-', Ied, ImB, 'b*-', Ied, Ied, 'k--'); grid on;
xlabel('Ie (cm)'); ylabel('Im (cm)'); title('Courbe d''etalonnage'); legend('A : 58 us/cm','B : adapte','ideale','Location','northwest');
nexttile; plot(Ied, dA, 'r*-', Ied, dB, 'b*-'); grid on; yline(0,'k:');
xlabel('Ie (cm)'); ylabel('\Delta d = Im - Ie (cm)'); title(sprintf('Cl A = %.2f %% ; Cl B = %.3f %%', ClA, ClB));
figure(6); plot(Tl, d_fixe-d0, 'r*-', Tl, d_comp-d0, 'b*-'); grid on; yline(0,'k:');
xlabel('Temperature T (degC)'); ylabel('erreur de distance (cm)'); legend('sans compensation','avec compensation LM35');
title('Compensation en temperature (d = 100 cm)');

Rth = R1*R2/(R1+R2);   f1 = 1/(2*pi*Rth*Cp);   f2 = GBW;
f   = logspace(2, 8, 600);
H   = Gn ./ ((1+1j*f/f1).*(1+1j*f/f2));
Gb1 = 20*log10(abs(H));   ph = angle(H)*180/pi;
G0  = 20*log10(Gn);
i3  = find(Gb1 <= G0-3, 1);
fc  = 10^(log10(f(i3-1)) + (G0-3-Gb1(i3-1))/(Gb1(i3)-Gb1(i3-1))*(log10(f(i3))-log10(f(i3-1))));
phc = interp1(log10(f), ph, log10(fc));
pente = (interp1(log10(f),Gb1,8) - interp1(log10(f),Gb1,7));
tau1 = 1/(2*pi*f1);   tau2 = 1/(2*pi*f2);
ts = (0:1e-9:10e-6)';
ys = 1 - (tau1*exp(-ts/tau1) - tau2*exp(-ts/tau2))/(tau1 - tau2);
t10 = ts(find(ys>=0.1,1));  t90 = ts(find(ys>=0.9,1));  tr = t90 - t10;
Vh  = min(Vcalth, Vcc-1.5);
yS  = suiveur(Vh*ones(size(ts)), 1e-9, tau2, SR, 0);
tS10 = ts(find(yS>=0.1*Vh,1));  tS90 = ts(find(yS>=0.9*Vh,1));  trS = tS90 - tS10;
tmin = 2*kModele;
dtS  = (Vh - 2*SEUIL*Vref_pcb/Nmax)/SR;
ddS  = v(T0)*dtS/2*1e3;
fprintf('\n=== PARTIE 4 : Bode et reponse temporelle ===\n');
fprintf('Erreur de pente (slew rate) : %.2f us soit %.2f mm\n', dtS*1e6, ddS);
fprintf('G0 = %.2f dB | f1 = %.0f kHz | f2 = %.0f kHz | fc(-3 dB) = %.0f kHz | phase(fc) = %.0f deg | pente = %.0f dB/dec\n', ...
        G0, f1/1e3, f2/1e3, fc/1e3, phc, pente);
fprintf('tau1 = %.0f ns | tau2 = %.0f ns | tr (petits signaux) = %.0f ns | tr (slew rate, %.2f V) = %.2f us | echo min = %d us\n', ...
        tau1*1e9, tau2*1e9, tr*1e9, Vh, trS*1e6, tmin);

figure(7); tiledlayout(2,1,'TileSpacing','compact');
nexttile; semilogx(f, Gb1, 'b', 'LineWidth',1.6); grid on;
yline(G0-3,'r--','G0 - 3 dB','LabelHorizontalAlignment','left'); xline(fc,'k:');
ylabel('Gb1 = 20 log|H| (dB)'); title({'Diagramme de Bode du conditionneur', ...
       sprintf('G0 = %.2f dB ; fc = %.0f kHz ; pente %.0f dB/decade', G0, fc/1e3, pente)});
nexttile; semilogx(f, ph, 'b', 'LineWidth',1.6); grid on; xline(fc,'k:');
xlabel('f (Hz)'); ylabel('phase (deg)');
figure(8); plot(ts*1e6, ys*Vh, 'b--', ts*1e6, yS, 'r', 'LineWidth',1.5); grid on; xlim([0 10]);
xlabel('t (us)'); ylabel('V_{PA0} (V)'); legend('lineaire (petits signaux)','avec slew rate (3,33 V)','Location','southeast');
title({'Reponse a un echelon du conditionneur', ...
       sprintf('tr = %.0f ns (lineaire) ; %.2f us (avec slew rate)', tr*1e9, trS*1e6)});

Rcan_pcb = Vref_pcb/Nmax*1e3;   Rcan_sim = Vref_sim/Nmax*1e3;
RT_pcb   = Vref_pcb/Nmax/Scap;
Nt = (cal_D*kModele)/(Tconv*1e6);   Rdist = cal_D/Nt;
fprintf('\n=== PARTIE 5 : resolution et incertitude ===\n');
fprintf('R(CAN) = %.3f mV (3,3 V) / %.3f mV (5 V) | R(T) = %.3f degC | R(distance) = %.3f cm\n', ...
        Rcan_pcb, Rcan_sim, RT_pcb, Rdist);
k = 2;  nrep = 10;  dtab = [50 100 200 400]';
uT = 0.5/sqrt(3);   umod = 0.3/sqrt(3);
res = zeros(numel(dtab), 6);  txt = strings(numel(dtab),1);
for q = 1:numel(dtab)
    dd = dtab(q);
    mi = (dd*kT + Tconv*1e6*(rand(nrep,1)-rand(nrep,1)))*v(T0)/20000;
    mbar = mean(mi);  sA = std(mi);  uA = sA/sqrt(nrep);  UA = k*uA;
    dDdT = 0.606*(dd*kT)/20000;
    uB = sqrt((dDdT*uT)^2 + umod^2);  UB = k*uB;
    U  = sqrt(UA^2 + UB^2);
    res(q,:) = [dd mbar sA UA UB U];
    txt(q) = fmtU(mbar, U);
    fprintf('d = %3d cm : m = %.3f | sA = %.3f | UA = %.3f | UB = %.3f | U = %.2f -> d = %s cm\n', ...
            dd, mbar, sA, UA, UB, U, txt(q));
end

for n = 1:8
    exportgraphics(figure(n), sprintf('cr_fig%d.png', n), 'Resolution', 130);
end
R.part1 = struct('Vini',Vini,'Voutmi',Voutmi,'Ncode',Ncode,'Vthi',Vthi,'Eigm',Eigm*100,'Voutmodi',Voutmodi, ...
    'Gn',Gn,'Vcalth',Vcalth,'a1',a1,'b1',b1,'Egmax',Egmax,'ELmax',ELmax,'Eoff',Eoff,'Sgmoy',Sgmoy,'Eli',Eli);
R.part2 = struct('Ie',Ie,'Nc',Nc,'Im_av',Im_av,'Im_ap',Im_ap,'dT_av',dT_av,'dT_ap',dT_ap,'Cl_av',Cl_av,'Cl_ap',Cl_ap, ...
    'Sg_pcb',Sg_pcb,'Sg_sim',Sg_sim,'pente',pT(1),'Scap',Scap,'SCAN_pcb',SCAN_pcb,'SCAN_sim',SCAN_sim);
R.part3 = struct('Ied',Ied,'ImA',ImA,'ImB',ImB,'dA',dA,'dB',dB,'erA',erA,'erB',erB,'ClA',ClA,'ClB',ClB,'SmoyA',SmoyA,'kT',kT, ...
    'Tl',Tl,'kTl',kTl,'d_fixe',d_fixe,'d_comp',d_comp);
R.part4 = struct('G0',G0,'f1',f1,'f2',f2,'fc',fc,'phc',phc,'pente',pente,'tau1',tau1,'tau2',tau2,'tr',tr,'trS',trS,'tmin',tmin,'Vh',Vh,'Rth',Rth,'dtS',dtS,'ddS',ddS);
R.part5 = struct('Rcan_pcb',Rcan_pcb,'Rcan_sim',Rcan_sim,'RT_pcb',RT_pcb,'Rdist',Rdist,'res',res,'txt',txt);
fid = fopen('resultats.json','w'); fwrite(fid, jsonencode(R)); fclose(fid);

function y = suiveur(x, dt, tau, SR, y0)
y = zeros(size(x));  y(1) = y0;
for k = 2:numel(x)
    dy = (x(k-1) - y(k-1))*dt/tau;
    dy = max(min(dy, SR*dt), -SR*dt);
    y(k) = y(k-1) + dy;
end
end

function s = fmtU(m, U)
nd = max(0, 1 - floor(log10(U)));
s = sprintf('%.*f +/- %.*f', nd, m, nd, U);
end

