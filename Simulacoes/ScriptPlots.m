% =========================================================================
% Script Mestre: Análise de Sinais do ESC (Extração Dinâmica)
% =========================================================================
clear; clc; close all;

%% 0. Configurações Iniciais
% Nome do arquivo exportado do LTSpice com todos os dados
filename = 'trifasico.txt'; 
pasta_saida = 'plot_simulacao';

% Criação da pasta de destino das imagens
if ~exist(pasta_saida, 'dir')
    mkdir(pasta_saida);
    disp(['Pasta "', pasta_saida, '" criada.']);
end

%% 1. Importação Robusta dos Dados
opts = detectImportOptions(filename);
opts.VariableNamingRule = 'preserve'; % Garante que os nomes originais sejam mantidos
data = readtable(filename, opts);

disp('Extraindo variáveis dinamicamente pelo cabeçalho...');

% O tempo é sempre a base.
t_us = data{:, 'time'} * 1e6; 

%% ========================================================================
% Figura 1: Correntes do Motor
% =========================================================================
fig1 = figure('Name', 'Correntes do Motor', 'Color', 'white', 'Position', [100, 100, 800, 400]);

plot(t_us, data{:, 'I(Rmotor1)'}, 'r-', 'LineWidth', 1.5); hold on;
plot(t_us, data{:, 'I(Rmotor2)'}, 'g-', 'LineWidth', 1.5);
plot(t_us, data{:, 'I(Rmotor3)'}, 'b-', 'LineWidth', 1.5); hold off;

xlabel('Tempo (\mu s)', 'FontSize', 11, 'FontWeight', 'bold');
ylabel('Corrente (A)', 'FontWeight', 'bold');
legend('I_A', 'I_B', 'I_C', 'Location', 'best', 'FontSize', 11);
xlim([0 max(t_us)]); % Trava o eixo X no tempo exato da simulação
grid on; grid minor; set(gca, 'Color', 'white', 'FontSize', 11, 'LineWidth', 1.2);

exportgraphics(fig1, fullfile(pasta_saida, '1_correntes_motor.png'), 'Resolution', 300, 'BackgroundColor', 'white');

%% ========================================================================
% Figura 2: Tensão entre Fase e GND (Ponte Trifásica)
% =========================================================================
fig2 = figure('Name', 'Tensão Fase-GND', 'Color', 'white', 'Position', [120, 120, 800, 400]);

plot(t_us, data{:, 'V(n002)'}, 'Color', [0.8 0 0], 'LineWidth', 1); hold on;
plot(t_us, data{:, 'V(n005)'}, 'Color', [0 0.5 0], 'LineWidth', 1);
plot(t_us, data{:, 'V(n004)'}, 'Color', [0 0 0.7], 'LineWidth', 1); hold off;

xlabel('Tempo (\mu s)', 'FontSize', 11, 'FontWeight', 'bold');
ylabel('Tensão (V)', 'FontWeight', 'bold');
legend('V_A', 'V_B', 'V_C', 'Location', 'best', 'FontSize', 11);
xlim([0 max(t_us)]);
grid on; grid minor; set(gca, 'Color', 'white', 'FontSize', 11, 'LineWidth', 1.2);

exportgraphics(fig2, fullfile(pasta_saida, '2_tensoes_fase_gnd.png'), 'Resolution', 300, 'BackgroundColor', 'white');

%% ========================================================================
% Figura 3: Tensão entre Fases e Neutro do Motor (BEMF / PWM Real)
% =========================================================================
fig3 = figure('Name', 'Tensão Fase-Neutro', 'Color', 'white', 'Position', [140, 140, 800, 400]);

plot(t_us, data{:, 'V(N002,N019)'}, 'Color', [0.8 0 0], 'LineWidth', 1); hold on;
plot(t_us, data{:, 'V(N005,N019)'}, 'Color', [0 0.5 0], 'LineWidth', 1);
plot(t_us, data{:, 'V(N004,N019)'}, 'Color', [0 0 0.7], 'LineWidth', 1); hold off;

xlabel('Tempo (\mu s)', 'FontSize', 11, 'FontWeight', 'bold');
ylabel('Tensão (V)', 'FontWeight', 'bold');
legend('V_{AN}', 'V_{BN}', 'V_{CN}', 'Location', 'best', 'FontSize', 11);
xlim([0 max(t_us)]);
grid on; grid minor; set(gca, 'Color', 'white', 'FontSize', 11, 'LineWidth', 1.2);

exportgraphics(fig3, fullfile(pasta_saida, '3_tensoes_fase_neutro.png'), 'Resolution', 300, 'BackgroundColor', 'white');

%% ========================================================================
% Figura 4: Vgs Fase A
% =========================================================================
fig4 = figure('Name', 'Vgs Fase A', 'Color', 'white', 'Position', [160, 160, 800, 400]);

plot(t_us, data{:, 'V(N008,N002)'}, 'b-', 'LineWidth', 1.5); hold on;
plot(t_us, data{:, 'V(n015)'}, 'r-', 'LineWidth', 1.5); hold off;

xlabel('Tempo (\mu s)', 'FontSize', 11, 'FontWeight', 'bold');
ylabel('Tensão (V)', 'FontWeight', 'bold');
legend('V_{gs} (High-Side)', 'V_{gs} (Low-Side)', 'Location', 'best', 'FontSize', 11);
xlim([0 max(t_us)]);
grid on; grid minor; set(gca, 'Color', 'white', 'FontSize', 11, 'LineWidth', 1.2);

exportgraphics(fig4, fullfile(pasta_saida, '4_vgs_fase_A.png'), 'Resolution', 300, 'BackgroundColor', 'white');

%% ========================================================================
% Figura 5: Vgs Fase B
% =========================================================================
fig5 = figure('Name', 'Vgs Fase B', 'Color', 'white', 'Position', [180, 180, 800, 400]);

plot(t_us, data{:, 'V(N009,N005)'}, 'b-', 'LineWidth', 1.5); hold on;
plot(t_us, data{:, 'V(n016)'}, 'r-', 'LineWidth', 1.5); hold off;

xlabel('Tempo (\mu s)', 'FontSize', 11, 'FontWeight', 'bold');
ylabel('Tensão (V)', 'FontWeight', 'bold');
legend('V_{gs} (High-Side)', 'V_{gs} (Low-Side)', 'Location', 'best', 'FontSize', 11);
xlim([0 max(t_us)]);
grid on; grid minor; set(gca, 'Color', 'white', 'FontSize', 11, 'LineWidth', 1.2);

exportgraphics(fig5, fullfile(pasta_saida, '5_vgs_fase_B.png'), 'Resolution', 300, 'BackgroundColor', 'white');

%% ========================================================================
% Figura 6: Vgs Fase C
% =========================================================================
fig6 = figure('Name', 'Vgs Fase C', 'Color', 'white', 'Position', [200, 200, 800, 400]);

plot(t_us, data{:, 'V(N011,N004)'}, 'b-', 'LineWidth', 1.5); hold on;
plot(t_us, data{:, 'V(n017)'}, 'r-', 'LineWidth', 1.5); hold off;

xlabel('Tempo (\mu s)', 'FontSize', 11, 'FontWeight', 'bold');
ylabel('Tensão (V)', 'FontWeight', 'bold');
legend('V_{gs} (High-Side)', 'V_{gs} (Low-Side)', 'Location', 'best', 'FontSize', 11);
xlim([0 max(t_us)]);
grid on; grid minor; set(gca, 'Color', 'white', 'FontSize', 11, 'LineWidth', 1.2);

exportgraphics(fig6, fullfile(pasta_saida, '6_vgs_fase_C.png'), 'Resolution', 300, 'BackgroundColor', 'white');

%% ========================================================================
% Figura 7: Bateria (Tensão e Corrente) - Eixos Duplos (yyaxis)
% =========================================================================
fig7 = figure('Name', 'Bateria DC', 'Color', 'white', 'Position', [220, 220, 800, 400]);

% Extrai a corrente invertendo o sinal para consumo positivo
I_bat_pos = data{:, 'I(Battery)'} * -1; 

% ---- Eixo Esquerdo: Tensão (Verde) ----
yyaxis left;
plot(t_us, data{:, 'V(n001)'}, 'Color', [0 0.5 0], 'LineWidth', 1.5);
ylabel('Tensão (V)', 'FontWeight', 'bold');

% ---- Eixo Direito: Corrente (Azul) ----
yyaxis right;
plot(t_us, I_bat_pos, 'Color', [0 0.2 0.8], 'LineWidth', 1.2);
ylabel('Corrente (A)', 'FontWeight', 'bold');

% Eixo X e Formatação Geral
xlabel('Tempo (\mu s)', 'FontSize', 11, 'FontWeight', 'bold');
xlim([0 max(t_us)]);

% Pinta os números dos eixos para combinar com as respectivas linhas
ax = gca;
ax.YAxis(1).Color = [0 0.5 0]; 
ax.YAxis(2).Color = [0 0.2 0.8]; 

grid on; grid minor; 
set(gca, 'Color', 'white', 'FontSize', 11, 'LineWidth', 1.2);

exportgraphics(fig7, fullfile(pasta_saida, '7_bateria_dc.png'), 'Resolution', 300, 'BackgroundColor', 'white');

disp('====================================================');
disp('Todos os gráficos foram gerados e salvos com sucesso.');
close all;