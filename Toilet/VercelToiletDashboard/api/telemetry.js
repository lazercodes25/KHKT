// Demo storage: Vercel Functions are stateless, so this is suitable for a
// quick demo only. Use a database/Redis when the dashboard must retain data.
let latest = {
  device_id: "toilet-01",
  received_at: new Date().toISOString(),
  values: {},
  events: []
};

const labels = {
  DATA0: "Phát hiện bạo lực",
  DATA1: "Khói thuốc",
  DATA2: "Yêu cầu dọn vệ sinh",
  DATA3: "Âm thanh lớn",
  DATA4: "Va chạm",
  DATA5: "Tiếng hét",
  DATA6: "Khí NH3",
  DATA7: "Số lần sử dụng vòi nước"
};

function authorized(req) {
  const expected = process.env.TOILET_DEVICE_KEY || "demo-key";
  return req.headers["x-device-key"] === expected;
}

export default function handler(req, res) {
  res.setHeader("Access-Control-Allow-Origin", "*");
  res.setHeader("Access-Control-Allow-Headers", "Content-Type, X-Device-Key");
  if (req.method === "OPTIONS") return res.status(204).end();

  if (req.method === "GET") return res.status(200).json(latest);
  if (req.method !== "POST") return res.status(405).json({ error: "Method not allowed" });
  if (!authorized(req)) return res.status(401).json({ error: "Invalid device key" });

  const body = typeof req.body === "string" ? JSON.parse(req.body) : req.body;
  const values = body.values || body;
  const receivedAt = new Date().toISOString();
  const events = Object.entries(values)
    .filter(([key, value]) => key.startsWith("DATA") && Number(value) > 0)
    .map(([key, value]) => ({ key, label: labels[key] || key, value, received_at: receivedAt }));

  latest = {
    device_id: body.device_id || "toilet-01",
    received_at: receivedAt,
    values: { ...latest.values, ...values },
    events: [...events, ...latest.events].slice(0, 30)
  };
  return res.status(200).json({ ok: true, received_at: receivedAt });
}
