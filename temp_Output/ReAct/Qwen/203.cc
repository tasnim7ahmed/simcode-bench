#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/animation-interface.h"
#include "ns3/v4ping-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetSimulation");

int main(int argc, char *argv[]) {
    uint32_t numVehicles = 5;
    double simDuration = 10.0;
    std::string phyMode("OfdmRate6_5MbpsBW10MHz");

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(phyMode));

    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiChannel.AddPropagationLoss("ns3::ThreeLogDistancePropagationLossModel");
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211p);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiChannel.Create(), vehicles);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    for (uint32_t i = 0; i < numVehicles; ++i) {
        Ptr<Node> node = vehicles.Get(i);
        Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
        Vector position = Vector(10.0 + i * 50.0, 0.0, 0.0);
        mobility->SetPosition(position);
        mobility->SetVelocity(Vector(20.0 + i * 2.0, 0.0, 0.0)); // varying speeds
    }

    InternetStackHelper stack;
    stack.Install(vehicles);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(vehicles.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simDuration));

    UdpClientHelper client(interfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < numVehicles; ++i) {
        clientApps.Add(client.Install(vehicles.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simDuration));

    AsciiTraceHelper ascii;
    wifi.EnableAsciiAll(ascii.CreateFileStream("vanet.tr"));
    wifi.EnablePcapAll("vanet");

    AnimationInterface anim("vanet-animation.xml");
    anim.SetMobilityPollInterval(Seconds(0.1));

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();

    UdpServer* serverPtr = dynamic_cast<UdpServer*>(serverApps.Get(0)->GetObject<Application>());
    if (serverPtr) {
        std::cout << "Total packets received: " << serverPtr->GetReceived() << std::endl;
        std::cout << "Packet Delivery Ratio: " << static_cast<double>(serverPtr->GetReceived()) / 4000.0 << std::endl; // 4 clients x 1000 packets
    }

    Simulator::Destroy();
    return 0;
}