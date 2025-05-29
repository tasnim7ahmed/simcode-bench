#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/energy-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/on-off-helper.h"
#include "ns3/udp-echo-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WsnEnergy");

int main (int argc, char *argv[])
{
  bool tracing = false;
  uint32_t gridSize = 5;
  double simulationTime = 100;

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable or disable tracing", tracing);
  cmd.AddValue ("gridSize", "Grid Size", gridSize);
  cmd.AddValue ("simulationTime", "Simulation Time", simulationTime);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer sensors;
  sensors.Create (gridSize * gridSize);
  NodeContainer sink;
  sink.Create (1);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer sensorDevices = wifi.Install (phy, mac, sensors);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer sinkDevices = wifi.Install (phy, mac, sink);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (5.0),
                                 "GridWidth", UintegerValue (gridSize),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (sensors);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (sink);
  Ptr<Node> sinkNode = sink.Get (0);
  Ptr<MobilityModel> sinkMobility = sinkNode->GetObject<MobilityModel> ();
  sinkMobility->SetPosition (Vector (gridSize * 2.5, gridSize * 2.5, 0));

  InternetStackHelper internet;
  internet.Install (sensors);
  internet.Install (sink);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer sensorInterfaces = ipv4.Assign (sensorDevices);
  Ipv4InterfaceContainer sinkInterfaces = ipv4.Assign (sinkDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  TypeId energySourceTypeId = TypeId::LookupByName ("ns3::LiIonEnergySource");
  EnergySourceHelper energySourceHelper;
  energySourceHelper.Set ("TypeId", TypeIdValue (energySourceTypeId));
  energySourceHelper.Set ("NominalVoltage", DoubleValue (3.6));
  energySourceHelper.Set ("InitialEnergy", DoubleValue (1000));

  BasicEnergySourceContainer energySources = energySourceHelper.Install (sensors);

  WifiRadioEnergyModelHelper radioEnergyModelHelper;
  radioEnergyModelHelper.Set ("TxCurrentA", DoubleValue (0.0174));
  radioEnergyModelHelper.Set ("RxCurrentA", DoubleValue (0.0197));
  radioEnergyModelHelper.Set ("SleepCurrentA", DoubleValue (0.0000015));
  radioEnergyModelHelper.Set ("IdleCurrentA", DoubleValue (0.0000015));

  DeviceEnergyModelContainer deviceModels = radioEnergyModelHelper.Install (sensorDevices, energySources);

  uint16_t port = 9;

  OnOffHelper onoff ("ns3::UdpSocketFactory",
                     Address (InetSocketAddress (sinkInterfaces.GetAddress (0), port)));
  onoff.SetConstantRate (DataRate ("5kbps"));

  ApplicationContainer apps;

  for (uint32_t i = 0; i < sensors.GetN (); ++i)
    {
      Ptr<Node> sensorNode = sensors.Get (i);
      Ptr<EnergySource> energySource = energySources.Get (i);
      Ptr<DeviceEnergyModel> deviceEnergyModel = deviceModels.Get (i);

      ApplicationContainer app = onoff.Install (sensorNode);
      apps.Add (app);

      Simulator::Schedule (Seconds (0.0), [&, energySource, deviceEnergyModel,sensorNode] () {
        NS_LOG_UNCOND("Node " << sensorNode->GetId() << "Energy source start");
      });

      energySource->TraceConnectWithoutContext ("RemainingEnergy",
                                                MakeCallback (&
                                                            [] (double oldValue, double newValue)
                                                            {
                                                              if (newValue <= 0)
                                                                {
                                                                  NS_LOG_UNCOND ("Node dies!");
                                                                  Simulator::Stop (Seconds(0));
                                                                }
                                                            }));

      deviceEnergyModel->TraceConnectWithoutContext ("TotalEnergyConsumption",
                                                     MakeCallback (&
                                                                  [] (double oldValue, double newValue)
                                                                  {
                                                                    NS_LOG_INFO ("Energy Consumption: " << newValue);
                                                                  }));
    }

  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (simulationTime));

  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (sink.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (simulationTime));

  if (tracing)
    {
      phy.EnablePcapAll ("wsn-energy");
    }

  Simulator::Stop (Seconds (simulationTime + 1));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}