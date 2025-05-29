#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

class BsmSenderApp : public Application
{
public:
  BsmSenderApp() {}
  virtual ~BsmSenderApp() {}

  void Setup(Ptr<WaveNetDevice> dev, Address dest, uint16_t port, Time interval)
  {
    m_device = dev;
    m_destAddr = dest;
    m_port = port;
    m_interval = interval;
  }

private:
  virtual void StartApplication() override
  {
    m_running = true;
    m_socket = Socket::CreateSocket(GetNode(), TypeId::LookupByName("ns3::UdpSocketFactory"));

    m_socket->Bind();
    m_socket->Connect(InetSocketAddress(InetSocketAddress::ConvertFrom(m_destAddr).GetIpv4(), m_port));

    SendBsm();
  }

  virtual void StopApplication() override
  {
    m_running = false;
    if (m_sendEvent.IsRunning())
      Simulator::Cancel(m_sendEvent);
    if (m_socket)
      m_socket->Close();
  }

  void SendBsm()
  {
    if (!m_running) return;

    Ptr<Packet> packet = Create<Packet>(100);
    m_socket->Send(packet);

    m_sendEvent = Simulator::Schedule(m_interval, &BsmSenderApp::SendBsm, this);
  }

  Ptr<WaveNetDevice> m_device;
  Ptr<Socket> m_socket;
  Address m_destAddr;
  uint16_t m_port;
  Time m_interval;
  EventId m_sendEvent;
  bool m_running = false;
};

class BsmReceiverApp : public Application
{
public:
  BsmReceiverApp() {}
  virtual ~BsmReceiverApp() {}

  void Setup(uint16_t port)
  {
    m_port = port;
  }

private:
  virtual void StartApplication() override
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&BsmReceiverApp::HandleRead, this));
  }

  virtual void StopApplication() override
  {
    if (m_socket)
      m_socket->Close();
  }

  void HandleRead(Ptr<Socket> socket)
  {
    while (Ptr<Packet> packet = socket->Recv())
    {
      // Optionally process BSM
    }
  }

  Ptr<Socket> m_socket;
  uint16_t m_port;
};

int main(int argc, char *argv[])
{
  uint32_t numVehicles = 5;
  double simTime = 20.0;
  double txRange = 300.0;
  uint16_t bsmPort = 4000;
  Time bsmInterval = Seconds(1.0);

  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer vehicles;
  vehicles.Create(numVehicles);

  YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
  YansWavePhyHelper wavePhy = YansWavePhyHelper::Default();
  wavePhy.SetChannel(waveChannel.Create());
  wavePhy.Set("TxPowerStart", DoubleValue(20.0));
  wavePhy.Set("TxPowerEnd", DoubleValue(20.0));

  NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default();
  WaveHelper waveHelper = WaveHelper::Default();
  NetDeviceContainer devices = waveHelper.Install(wavePhy, waveMac, vehicles);

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                               "MinX", DoubleValue(0.0),
                               "MinY", DoubleValue(20.0),
                               "DeltaX", DoubleValue(60.0),
                               "DeltaY", DoubleValue(0.0),
                               "GridWidth", UintegerValue(numVehicles),
                               "LayoutType", StringValue("RowFirst"));
  mobility.Install(vehicles);

  // Assign velocity to each vehicle
  for (uint32_t i = 0; i < vehicles.GetN(); ++i)
  {
    Ptr<ConstantVelocityMobilityModel> mob = vehicles.Get(i)->GetObject<ConstantVelocityMobilityModel>();
    mob->SetVelocity(Vector(15.0, 0.0, 0.0)); // 15 m/s in X
  }

  InternetStackHelper internet;
  internet.Install(vehicles);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  // Install apps: each vehicle sends to all others, and is also a receiver
  for (uint32_t i = 0; i < numVehicles; ++i)
  {
    // Receiver
    Ptr<BsmReceiverApp> recvApp = CreateObject<BsmReceiverApp>();
    recvApp->Setup(bsmPort);
    vehicles.Get(i)->AddApplication(recvApp);
    recvApp->SetStartTime(Seconds(0.0));
    recvApp->SetStopTime(Seconds(simTime));

    // Senders: send BSM to every other vehicle
    for (uint32_t j = 0; j < numVehicles; ++j)
    {
      if (i == j) continue;
      Ptr<BsmSenderApp> sendApp = CreateObject<BsmSenderApp>();
      Ptr<WaveNetDevice> dev = DynamicCast<WaveNetDevice>(devices.Get(i));
      Address dest = InetSocketAddress(interfaces.GetAddress(j), bsmPort);
      sendApp->Setup(dev, dest, bsmPort, bsmInterval);
      vehicles.Get(i)->AddApplication(sendApp);
      sendApp->SetStartTime(Seconds(0.1));
      sendApp->SetStopTime(Seconds(simTime));
    }
  }

  AnimationInterface anim("vanet-netanim.xml");
  for (uint32_t i = 0; i < numVehicles; ++i)
  {
    anim.UpdateNodeDescription(vehicles.Get(i), "Vehicle" + std::to_string(i));
    anim.UpdateNodeColor(vehicles.Get(i), 0, 255, 0); // Green
  }
  anim.SetMaxPktsPerTraceFile(500000);

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}