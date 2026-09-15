function [x,fs] = tascar_measure_radiationpattern( varargin )
    sCfg.type = 'cardioidmod';
    sHelp.type = 'radiation pattern type';
    sCfg.fmin = 62.5;
    sHelp.fmin = 'lower frequency range in Hz';
    sCfg.fmax = 8000;
    sHelp.fmax = 'upper frequency range in Hz';
    sCfg.bpo = 3;
    sHelp.bpo = 'bands per octave';
    sCfg.rhomin = -30;
    sHelp.rhomin = 'minimum radius in dB';
    sCfg.rhomax = 0;
    sHelp.rhomin = 'maximum radius in dB';
    sCfg.rhostep = 6;
    sHelp.rhostep = 'radius step size in dB';
    sCfg.plane = 'transverse';
    sHelp.plane = 'Analysis plane (transverse|sagittal|coronal)';
    sCfg = tascar_parse_keyval( sCfg, sHelp, varargin{:} );

    n_doc = tascar_xml_doc_new();
    % session (root element):
    n_session = tascar_xml_add_element(n_doc,n_doc,'session');
    n_scene = tascar_xml_add_element(n_doc,n_session,'scene');
    % source
    n_src = tascar_xml_add_element(n_doc, n_scene, 'source');
    n_sound = tascar_xml_add_element(n_doc, n_src, 'sound',[],...
                                     'type',sCfg.type);
    %,...
    %                                 'f6db','4000',...
    %                                 'fmin','500'
    % receiver:
    N = 72;
    N = 180;
    vaz = angle(exp(i*(2*pi*([1:N]-1)/N + pi)));
    fmt.transverse = '0 %g %g 0';
    fmt.sagittal = '0 %g 0 %g';
    fmt.coronal = '0 0 %g %g';
    dirs.transverse = {'front','left','back','right'};
    dirs.sagittal = {'front','top','back','bottom'};
    dirs.coronal = {'left','top','right','bottom'};
    for n=1:N
        az = vaz(n);
        n_rec = tascar_xml_add_element(n_doc,n_scene,'receiver',[],...
                                       'name', sprintf('out_%d',n));
        tascar_xml_add_element(n_doc, n_rec, ...
                               'position',sprintf(fmt.(sCfg.plane),...
                                                  cos(az),sin(az)));
    end
    system('rm -f temp_rec.wav temp_rec.tsc');
    tascar_xml_save( n_doc, 'temp_rec.tsc' );
    system('tascar_renderir temp_rec.tsc -o temp_rec.wav');
    [x,fs] = audioread('temp_rec.wav');
    cf = 1000*2.^[log2(sCfg.fmin/1000):(1/sCfg.bpo):log2(sCfg.fmax/1000)];
    bw = 1/sCfg.bpo;
    ef_low = round(cf * 2.^(-0.5*bw));
    ef_high = round(cf * 2.^(0.5*bw));
    H = realfft(x);
    m = zeros(size(x,2),numel(cf));
    for k=1:numel(cf)
        m(:,k) = 10*log10(mean(abs(H(ef_low(k):ef_high(k),:)).^2));
    end
    %figure
    %imagesc(m);
    %colorbar;
    %set(gca,'YTick',1:3:N,'YTickLabel',round(180/pi*vaz(1:3:N)),...
    %        'XTick',1:3:numel(cf),'XTickLabel',round(cf(1:3:end)));
    sName = sprintf('radiation_%s_%s',sCfg.type,sCfg.plane);
    sTitle = sprintf('%s (%s plane)',sCfg.type,sCfg.plane);
    figure('menubar','none','Name',sName, 'Units','centimeters','paperposition',[0 0 10 10]*0.5, 'position',[0,0,10,10]*0.5,'PaperSize',[10 10]*0.5);
    plot((sCfg.rhomax-sCfg.rhomin)*[-1,1],[0,0],'k-');
    hold on
    plot([0,0],(sCfg.rhomax-sCfg.rhomin)*[-1,1],'k-');
    for rho=sCfg.rhomax:-sCfg.rhostep:sCfg.rhomin
        c = (rho-sCfg.rhomin) .* exp(i * vaz(:));
        c(end+1) = c(1);
        plot( real(c), imag(c), 'k-','Color',[0.5,0.5,0.5]);
    end
    for az=[30,60,120,150]
        c = (sCfg.rhomax-sCfg.rhomin) .* exp(i * az * pi/180);
        plot(real(c)*[-1,1], imag(c)*[-1,1], '--','Color',[0.5,0.5,0.5]);
    end
    csLabel = {};
    vpl = [];
    for k=1:numel(cf)
        c = (max(0,-sCfg.rhomin + m(:,k))) .* exp(i * vaz(:));
        c(end+1) = c(1);
        vpl(end+1) = plot( real(c), imag(c), '-', 'linewidth',2);
        csLabel{end+1} = sprintf('%1.5g Hz',cf(k));
        hold on
    end
    set(gca,'DataAspectRatio',[1,1,1],'visible','off',...
            'xlim',(sCfg.rhomax-sCfg.rhomin)*1.1*[-1,1],...
            'ylim',(sCfg.rhomax-sCfg.rhomin)*1.1*[-1,1],...
       'Units','centimeters','position',[0,0,9.8,9.8]*0.5);

    for rho=sCfg.rhomax:-sCfg.rhostep:sCfg.rhomin
        x = (rho-sCfg.rhomin)*cos(82*pi/180);
        y = (rho-sCfg.rhomin)*sin(82*pi/180);
        text(x,y,sprintf(' %1.2g dB',rho),'verticalAlignment','bottom');
    end
    csDirs = dirs.(sCfg.plane);
    vRot = [90,0,270,0];
    csVAlign = {'top','bottom','top','top'};
    for k=1:4
        az = (k-1)*90;
        x = (sCfg.rhomax-sCfg.rhomin)*cos(az*pi/180)*1.02;
        y = (sCfg.rhomax-sCfg.rhomin)*sin(az*pi/180)*1.02;
        text(x,y,csDirs{k},'rotation',vRot(k), ...
             'horizontalAlignment','center','verticalAlignment',csVAlign{k});
    end
    %text(-(sCfg.rhomax-sCfg.rhomin),0,'back','rotation',-90,'horizontalAlignment','center','verticalAlignment','top');
    
    legend(vpl,csLabel);%,'Location','BestOutside');
                        %title(sTitle);
    saveas(gca,[sName,'.pdf'],'pdf');
end
