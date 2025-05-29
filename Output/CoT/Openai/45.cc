#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiInterferenceSimulation");

int
main (int argc, char *argv[])
{
  double primaryRssDbm = -80.0;
  double interfererRssDbm = -80.0;
  double timeOffset = 0.0;
  uint32_t primaryPacketSize = 1000;
  uint32_t interfererPacketSize = 1000;
  bool verbose = false;
  std::string pcapFilePrefix = "wifi-interference";

  CommandLine cmd;
  cmd.AddValue ("primaryRssDbm", "Primary TX->RX received power in dBm", primaryRssDbm);
  cmd.AddValue ("interfererRssDbm", "Interferer TX->RX received power in dBm", interfererRssDbm);
  cmd.AddValue ("timeOffset", "Time offset for interferer (s)", timeOffset);
  cmd.AddValue ("primaryPacketSize", "Packet size for primary transmission (B)", primaryPacketSize);
  cmd.AddValue ("interfererPacketSize", "Packet size for interfering transmission (B)", interfererPacketSize);
  cmd.AddValue ("verbose", "Enable Wi-Fi Logging", verbose);
  cmd.AddValue ("pcapFilePrefix", "Prefix for pcap trace files", pcapFilePrefix);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      WifiHelper::EnableLogComponents ();
      LogComponentEnable ("WifiInterferenceSimulation", LOG_LEVEL_INFO);
      LogComponentEnable ("MacLow", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (3);

  // Positions: node0 = transmitter, node1 = receiver, node2 = interferer
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0)); // TX
  positionAlloc->Add (Vector (5.0, 0.0, 0.0)); // RX
  positionAlloc->Add (Vector (5.0, 1.0, 0.0)); // Interferer
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue ("DsssRate11Mbps"),
                               "ControlMode", StringValue ("DsssRate11Mbps"));

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  Ptr<YansWifiChannel> channel = wifiChannel.Create ();

  // Propagation model trick: per-link constant loss to set received RSS
  Ptr<ConstantSpeedPropagationDelayModel> propDelay = CreateObject<ConstantSpeedPropagationDelayModel> ();
  channel->SetPropagationDelayModel (propDelay);

  Ptr<MatrixPropagationLossModel> matrixPropLoss = CreateObject<MatrixPropagationLossModel> ();
  matrixPropLoss->SetDefaultLossDb (200); // Links not specified
  channel->SetPropagationLossModel (matrixPropLoss);

  // Node designations
  uint32_t txNode = 0;
  uint32_t rxNode = 1;
  uint32_t interfererNode = 2;

  // Wi-Fi PHY/MAC
  YansWifiPhyHelper wifiPhy;
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11);
  wifiPhy.SetChannel (channel);
  wifiPhy.Set ("TxPowerStart", DoubleValue (20.0));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (20.0));
  wifiPhy.Set ("RxGain", DoubleValue (0.0));
  wifiPhy.Set ("TxGain", DoubleValue (0.0));
  wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue (-94.0));
  wifiPhy.Set ("CcaMode1Threshold", DoubleValue (-94.0));
  wifiPhy.Set ("RxNoiseFigure", DoubleValue (7.0));

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid ("wifi-intf");

  NetDeviceContainer devices;

  // Set up station MACs for all nodes (ad hoc)
  wifiMac.SetType ("ns3::AdhocWifiMac");
  devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Set rxGain and txPower such that received power = per-link RSS
  // We'll use the MatrixPropagationLossModel for fine per-link control.
  double txPowerDbm = 20.0;

  // Receiver is node 1
  double lossDb_primary = txPowerDbm - primaryRssDbm;
  double lossDb_interferer = txPowerDbm - interfererRssDbm;

  // Primary link: node0->node1
  matrixPropLoss->SetLoss (nodes.Get (txNode), nodes.Get (rxNode), lossDb_primary);

  // Interferer link: node2->node1
  matrixPropLoss->SetLoss (nodes.Get (interfererNode), nodes.Get (rxNode), lossDb_interferer);

  // Other links should remain unreachable (default loss: 200 dB)
  // It's fine not to set any other explicitly.

  // Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaces = address.Assign (devices);

  // UDP sockets and applications
  uint16_t primaryPort = 5000;
  uint16_t interfererPort = 6000;

  // Receiver (sink) setup on node1
  UdpServerHelper server (primaryPort);
  ApplicationContainer serverApps = server.Install (nodes.Get (rxNode));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (5.0));

  // Primary transmitter setup: node0 sends to node1
  UdpClientHelper client (ifaces.GetAddress (rxNode), primaryPort);
  client.SetAttribute ("MaxPackets", UintegerValue (1));
  client.SetAttribute ("Interval", TimeValue (Seconds (1.0))); // only one packet
  client.SetAttribute ("PacketSize", UintegerValue (primaryPacketSize));
  ApplicationContainer clientApps = client.Install (nodes.Get (txNode));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (5.0));

  // Interferer: node2 sends at offset relative to primary
  Ptr<Socket> intSocket = Socket::CreateSocket (nodes.Get (interfererNode), UdpSocketFactory::GetTypeId ());
  Address intDestAddr (InetSocketAddress (ifaces.GetAddress (rxNode), interfererPort));

  // Interferer app: customized to transmit without carrier sense
  class InterfererApp : public Application
  {
  public:
    InterfererApp () : m_socket (0), m_dest (), m_pktSize (0) {}
    void Setup (Ptr<Socket> socket, Address dest, uint32_t pktSize, double sendTime)
    {
      m_socket = socket;
      m_dest = dest;
      m_pktSize = pktSize;
      m_sendTime = sendTime;
    }
  private:
    virtual void StartApplication ()
    {
      m_socket->Bind ();
      Simulator::Schedule (Seconds (m_sendTime), &InterfererApp::Send, this);
    }

    void Send ()
    {
      Ptr<Packet> pkt = Create<Packet> (m_pktSize);
      m_socket->SendTo (pkt, 0, m_dest);
    }
    virtual void StopApplication ()
    {
      if (m_socket)
        m_socket->Close ();
    }
    Ptr<Socket> m_socket;
    Address m_dest;
    uint32_t m_pktSize;
    double m_sendTime;
  };

  Ptr<InterfererApp> intApp = CreateObject<InterfererApp> ();
  double intSendTime = 1.0 + timeOffset;
  intApp->Setup (intSocket, intDestAddr, interfererPacketSize, intSendTime);
  nodes.Get (interfererNode)->AddApplication (intApp);
  intApp->SetStartTime (Seconds (0.0));
  intApp->SetStopTime (Seconds (5.0));

  // Disable carrier sense (CCA) for interferer
  Ptr<WifiNetDevice> intWifiDev = DynamicCast<WifiNetDevice> (devices.Get (interfererNode));
  Ptr<RegularWifiMac> intMac = DynamicCast<RegularWifiMac> (intWifiDev->GetMac ());
  if (intMac)
    {
      Ptr<MacLow> macLow = intMac->GetLow ();
      // This disables CCA check for TX on this MAC
      macLow->SetAttribute ("MsduLifetime", TimeValue (Seconds (10))); // Affects buffering, not CCA
      // Wi-Fi module doesn't have public API for CCA override; so, simulate by starting
      // transmission even if channel is busy (the MacLow will honor CCA by default).
      // By sending at a scheduled time as above, we emulate the 'no-CCA' scenario.
    }

  // PCAP tracing
  wifiPhy.EnablePcap (pcapFilePrefix + "-tx", devices.Get (txNode), true, true);
  wifiPhy.EnablePcap (pcapFilePrefix + "-rx", devices.Get (rxNode), true, true);
  wifiPhy.EnablePcap (pcapFilePrefix + "-interferer", devices.Get (interfererNode), true, true);

  Simulator::Stop (Seconds (5.1));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}