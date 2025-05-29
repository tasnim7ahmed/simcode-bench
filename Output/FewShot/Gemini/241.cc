#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/ocb-wifi-mac.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes: vehicle and RSU
    NodeContainer nodes;
    nodes.Create(2);
    NodeContainer vehicleNodes = NodeContainer(nodes.Get(0));
    NodeContainer rsuNodes = NodeContainer(nodes.Get(1));

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicleNodes);
    Ptr<ConstantVelocityMobilityModel> vehMob = vehicleNodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    vehMob->SetVelocity(Vector(10, 0, 0)); // 10 m/s
    mobility.Install(rsuNodes);
    
    // Wifi 802.11p (WAVE)
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    Wifi80211pHelper wifi80211pHelper;
    QosWaveMacHelper wifiMac = QosWaveMacHelper::Default();
    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue("OfdmRate6MbpsBW10MHz"),
                                "ControlMode",StringValue("OfdmRate6MbpsBW10MHz"));

    NetDeviceContainer devices = wifi80211pHelper.Install(wifiPhy, wifiMac, nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Server (RSU)
    UdpServerHelper server(9);
    ApplicationContainer serverApp = server.Install(rsuNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Client (Vehicle)
    UdpClientHelper client(interfaces.GetAddress(1), 9);
    client.SetAttribute("MaxPackets", UintegerValue(10));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(vehicleNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}