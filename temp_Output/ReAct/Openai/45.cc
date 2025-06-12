#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiInterferenceSimulation");

int main (int argc, char *argv[])
{
  double primaryRssDbm = -80.0;
  double interfererRssDbm = -80.0;
  double interfererOffset = 0.0;
  uint32_t primaryPktSize = 1024;
  uint32_t interfererPktSize = 1024;
  bool verbose = false;

  CommandLine cmd;
  cmd.AddValue ("primaryRssDbm", "Received signal strength (dBm) at receiver from transmitter", primaryRssDbm);
  cmd.AddValue ("interfererRssDbm", "Received signal strength (dBm) at receiver from interferer", interfererRssDbm);
  cmd.AddValue ("interfererOffset", "Time (s) by which interferer transmission is offset", interfererOffset);
  cmd.AddValue ("primaryPktSize", "Packet size (bytes) for transmitter", primaryPktSize);
  cmd.AddValue ("interfererPktSize", "Packet size (bytes) for interferer", interfererPktSize);
  cmd.AddValue ("verbose", "Enable verbose Wi-Fi logging", verbose);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      WifiHelper::EnableLogComponents ();
      LogComponentEnable ("WifiInterferenceSimulation", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (3); // 0: Tx, 1: Rx, 2: Interferer

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();

  wifiPhy.SetChannel (wifiChannel.Create ());
  wifiPhy.Set ("TxPowerStart", DoubleValue (0));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (0));
  wifiPhy.Set ("TxPowerLevels", UintegerValue (1));
  wifiPhy.Set ("RxGain", DoubleValue (0));
  wifiPhy.Set ("TxGain", DoubleValue (0));
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Place Tx at (0,0,0), Rx at (1,0,0). Interferer can be placed far but we'll use path loss to set RSS.
  Ptr<MobilityModel> txMob = nodes.Get (0)->GetObject<MobilityModel> ();
  txMob->SetPosition (Vector (0.0, 0.0, 0.0));
  Ptr<MobilityModel> rxMob = nodes.Get (1)->GetObject<MobilityModel> ();
  rxMob->SetPosition (Vector (1.0, 0.0, 0.0));
  Ptr<MobilityModel> intMob = nodes.Get (2)->GetObject<MobilityModel> ();
  intMob->SetPosition (Vector (2.0, 0.0, 0.0));

  // Set propagation loss to achieve requested RSS for both links (Tx->Rx and Interferer->Rx), set others to minimum
  // Use MatrixPropagationLossModel for explicit control
  Ptr<YansWifiChannel> channel = DynamicCast<YansWifiChannel>(wifiPhy.GetChannel ());
  Ptr<MatrixPropagationLossModel> matrix = CreateObject<MatrixPropagationLossModel> ();
  matrix->SetDefaultLossDb (200); // Set a large default loss
  // IDs: 0=Tx, 1=Rx, 2=Interferer
  double txPowerDbm = 0.0; // see above (TxPowerStart/End)
  double rxNoise = -97.0; // not used, reference
  double txToRxLoss = txPowerDbm - primaryRssDbm;
  double intToRxLoss = txPowerDbm - interfererRssDbm;
  matrix->SetLoss (nodes.Get (0)->GetObject<MobilityModel>(), nodes.Get (1)->GetObject<MobilityModel>(), txToRxLoss);
  matrix->SetLoss (nodes.Get (2)->GetObject<MobilityModel>(), nodes.Get (1)->GetObject<MobilityModel>(), intToRxLoss);
  matrix->SetLoss (nodes.Get (0)->GetObject<MobilityModel>(), nodes.Get (2)->GetObject<MobilityModel>(), 200); // for completeness
  matrix->SetLoss (nodes.Get (1)->GetObject<MobilityModel>(), nodes.Get (0)->GetObject<MobilityModel>(), 200);
  matrix->SetLoss (nodes.Get (2)->GetObject<MobilityModel>(), nodes.Get (0)->GetObject<MobilityModel>(), 200);
  matrix->SetLoss (nodes.Get (1)->GetObject<MobilityModel>(), nodes.Get (2)->GetObject<MobilityModel>(), 200);

  channel->SetPropagationLossModel (matrix);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Disable carrier sense for interferer (TxOp won't check CCA, but we can disable DCF by using NoAck WifiMac or via packet injection)
  // Best is to use a RawSocket on the interferer node, but for simplicity, we'll use UdpEchoServer/Client and configure the MAC.

  uint16_t port = 9;

  // UDP Echo Server on Rx node
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

  // Primary Transmitter
  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0))); // irrelevant (just one packet)
  echoClient.SetAttribute ("PacketSize", UintegerValue (primaryPktSize));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (1.0)); // Start at t=1s
  clientApps.Stop (Seconds (10.0));

  // Interferer sends to Rx at the same port
  UdpEchoClientHelper interfererClient (interfaces.GetAddress (1), port);
  interfererClient.SetAttribute ("MaxPackets", UintegerValue (1));
  interfererClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  interfererClient.SetAttribute ("PacketSize", UintegerValue (interfererPktSize));
  ApplicationContainer interfererApps = interfererClient.Install (nodes.Get (2));
  interfererApps.Start (Seconds (1.0 + interfererOffset)); // Offset relative to the primary transmission
  interfererApps.Stop (Seconds (10.0));

  // Disable CCA on interferer (node 2)
  Ptr<NetDevice> interfererDev = devices.Get (2);
  Ptr<WifiNetDevice> wifiInterferer = DynamicCast<WifiNetDevice> (interfererDev);
  Ptr<RegularWifiMac> mac = DynamicCast<RegularWifiMac> (wifiInterferer->GetMac ());
  if (mac)
    {
      Ptr<DcaTxop> dca = mac->GetDcaTxop ();
      dca->SetMinCw (0);
      dca->SetMaxCw (0);
      dca->SetAifsn (1);
      dca->SetAttribute ("DisableCca", BooleanValue (true)); // NS-3 >=3.41
    }

  // Generate pcap
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  wifiPhy.EnablePcap ("wifi-interference", devices);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}