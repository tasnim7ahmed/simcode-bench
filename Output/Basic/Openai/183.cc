#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ssid.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/internet-apps-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t numAps = 2;
    uint32_t numStas = 4;
    double simTime = 10.0;

    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer apNodes;
    apNodes.Create(numAps);
    NodeContainer staNodes;
    staNodes.Create(numStas);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper mac;
    Ssid ssid1 = Ssid("ssid-ap1");
    Ssid ssid2 = Ssid("ssid-ap2");

    NetDeviceContainer apDevices;
    NetDeviceContainer staDevices1;
    NetDeviceContainer staDevices2;

    // Mobility
    MobilityHelper mobility;

    // Place APs at (0,0,0) and (50,0,0)
    Ptr<ListPositionAllocator> apPosAlloc = CreateObject<ListPositionAllocator>();
    apPosAlloc->Add(Vector(0.0, 0.0, 0.0));
    apPosAlloc->Add(Vector(50.0, 0.0, 0.0));
    mobility.SetPositionAllocator(apPosAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);

    // Place STAs closer to their APs: 2 STAs per AP
    Ptr<ListPositionAllocator> staPosAlloc = CreateObject<ListPositionAllocator>();
    staPosAlloc->Add(Vector(5.0, 5.0, 0.0));   // STA 0 - AP 0
    staPosAlloc->Add(Vector(-5.0, -5.0, 0.0)); // STA 1 - AP 0
    staPosAlloc->Add(Vector(55.0, 5.0, 0.0));  // STA 2 - AP 1
    staPosAlloc->Add(Vector(45.0, -5.0, 0.0)); // STA 3 - AP 1
    mobility.SetPositionAllocator(staPosAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes);

    // Install AP devices
    // AP 0
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    apDevices.Add(wifi.Install(phy, mac, apNodes.Get(0)));
    // AP 1
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    apDevices.Add(wifi.Install(phy, mac, apNodes.Get(1)));

    // Install STA devices, connecting to nearest AP
    // STA 0 & 1 to AP 0 (ssid1)
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid1),
                "ActiveProbing", BooleanValue(false));
    staDevices1 = wifi.Install(phy, mac, NodeContainer(staNodes.Get(0), staNodes.Get(1)));
    // STA 2 & 3 to AP 1 (ssid2)
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid2),
                "ActiveProbing", BooleanValue(false));
    staDevices2 = wifi.Install(phy, mac, NodeContainer(staNodes.Get(2), staNodes.Get(3)));

    NetDeviceContainer staDevices;
    staDevices.Add(staDevices1);
    staDevices.Add(staDevices2);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    // IP addressing
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // UDP Traffic
    // UDP server on STA 3, UDP client on STA 0
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(staNodes.Get(3));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simTime));

    UdpClientHelper client(staInterfaces.GetAddress(3), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.05)));
    client.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApp = client.Install(staNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simTime - 1.0));

    // UDP server on STA 0, UDP client on STA 3
    UdpServerHelper server2(port+1);
    ApplicationContainer serverApp2 = server2.Install(staNodes.Get(0));
    serverApp2.Start(Seconds(1.0));
    serverApp2.Stop(Seconds(simTime));

    UdpClientHelper client2(staInterfaces.GetAddress(0), port+1);
    client2.SetAttribute("MaxPackets", UintegerValue(1000));
    client2.SetAttribute("Interval", TimeValue(Seconds(0.05)));
    client2.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApp2 = client2.Install(staNodes.Get(3));
    clientApp2.Start(Seconds(2.0));
    clientApp2.Stop(Seconds(simTime - 1.0));

    // NetAnim
    AnimationInterface anim("wifi-2ap-4sta.xml");
    anim.SetBackgroundImage("background.png", 0, 0, 100, 50, 1);

    anim.UpdateNodeDescription(apNodes.Get(0), "AP0");
    anim.UpdateNodeColor(apNodes.Get(0), 255, 0, 0);
    anim.UpdateNodeSize(apNodes.Get(0), 40, 40);

    anim.UpdateNodeDescription(apNodes.Get(1), "AP1");
    anim.UpdateNodeColor(apNodes.Get(1), 0, 0, 255);
    anim.UpdateNodeSize(apNodes.Get(1), 40, 40);

    for (uint32_t i = 0; i < staNodes.GetN(); ++i)
    {
        std::ostringstream oss;
        oss << "STA" << i;
        anim.UpdateNodeDescription(staNodes.Get(i), oss.str());
        anim.UpdateNodeColor(staNodes.Get(i), 0, 200, 0);
        anim.UpdateNodeSize(staNodes.Get(i), 20, 20);
    }

    // Enable packet metadata for animation
    phy.EnablePcap("wifi-2ap-4sta", apDevices);

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}