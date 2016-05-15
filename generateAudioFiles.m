Fs = 44100;
T = 1/Fs;
dur = 8;
t = 0:T:dur-T;

freqs = 100*logspace(0,2,1000);
% x = sin(2*pi.*t*f.*logspace(0,1,length(t)))*0.99;
fadeLength = 10000;
fade = ones(size(t));
fade(1:fadeLength) = (1:fadeLength)/fadeLength;
fade(end-fadeLength+1:end) = (fadeLength:-1:1)/fadeLength;
for n = 1 : length(freqs)
    f = freqs(n);
    x = sin(2*pi.*t*f)*0.99.*fade; 
    filename = sprintf('sin%04d.bin', n);
    fid = fopen(filename, 'w');
    out = round(x*32768);
    fwrite(fid, out, 'integer*2');
    fclose(fid);
end
