
vec3 tonemapFilmic(vec3 x) {
  vec3 X = max(vec3(0.0), x - 0.004);
  vec3 result = (X * (6.2 * X + 0.5)) / (X * (6.2 * X + 1.7) + 0.06);
  return pow(result, vec3(2.2));
}

vec3 tonemapUnreal(vec3 x) {
  return x / (x + 0.155) * 1.019;
}

vec3 uchimura(vec3 x, float P, float a, float m, float l, float c, float b) {
  float l0 = ((P - m) * l) / a;
  float L0 = m - m / a;
  float L1 = m + (1.0 - m) / a;
  float S0 = m + l0;
  float S1 = m + a * l0;
  float C2 = (a * P) / (P - S1);
  float CP = -C2 / P;

  vec3 w0 = vec3(1.0 - smoothstep(0.0, m, x));
  vec3 w2 = vec3(step(m + l0, x));
  vec3 w1 = vec3(1.0 - w0 - w2);

  vec3 T = vec3(m * pow(x / m, vec3(c)) + b);
  vec3 S = vec3(P - (P - S1) * exp(CP * (x - S0)));
  vec3 L = vec3(m + a * (x - m));

  return T * w0 + L * w1 + S * w2;
}


uniform float unUchMaxDisplayBrightness = 1.0;
uniform float unUchContrast = 1.0;
uniform float unUchLinearSectionStart = 0.22;
uniform float unUchLinearSectionLength = 0.4;
uniform float unUchBlack = 1.33;
uniform float unUchPedestal = 0.0;

vec3 tonemapUchimura(vec3 x) {
  const float P = unUchMaxDisplayBrightness;  // max display brightness
  const float a = unUchContrast;  // contrast
  const float m = unUchLinearSectionStart; // linear section start
  const float l = unUchLinearSectionLength;  // linear section length
  const float c = unUchBlack; // black
  const float b = unUchPedestal;  // pedestal

  return uchimura(x, P, a, m, l, c, b);
}

vec3 tonemapLottes(vec3 x) {
  const vec3 a = vec3(1.6);
  const vec3 d = vec3(0.977);
  const vec3 hdrMax = vec3(8.0);
  const vec3 midIn = vec3(0.18);
  const vec3 midOut = vec3(0.267);

  const vec3 b =
      (-pow(midIn, a) + pow(hdrMax, a) * midOut) /
      ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);
  const vec3 c =
      (pow(hdrMax, a * d) * pow(midIn, a) - pow(hdrMax, a) * pow(midIn, a * d) * midOut) /
      ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);

  return pow(x, a) / (pow(x, a * d) * b + c);
}

vec3 uncharted2Tonemap(vec3 x) {
  float A = 0.15;
  float B = 0.50;
  float C = 0.10;
  float D = 0.20;
  float E = 0.02;
  float F = 0.30;
  float W = 11.2;
  return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 tonemapUncharted2(vec3 color, float exposure) {
  const float W = 11.2;
  float exposureBias = exposure * 2.0;
  vec3 curr = uncharted2Tonemap(exposureBias * color);
  vec3 whiteScale = 1.0 / uncharted2Tonemap(vec3(W));
  return curr * whiteScale;
}

vec3 tonemapReinhard2(vec3 x, float exposure) {
  const float L_white = exposure * 4.0;

  return (x * (1.0 + x / (L_white * L_white))) / (1.0 + x);
}


vec3 computeTonemapping(vec3 inColor, float exposure, int tonemapOperator) {

    switch (tonemapOperator) {
        case 1:
            // reinhard tone mapping
            return tonemapReinhard2(inColor, exposure);
        case 2:
            return tonemapLottes(inColor);
        case 3:
            return tonemapUchimura(inColor);
        case 4:
            return tonemapUnreal(inColor);
        case 5:
            return tonemapFilmic(inColor);
        case 6:
            return tonemapUncharted2(inColor, exposure);
    }
    return inColor;
}
