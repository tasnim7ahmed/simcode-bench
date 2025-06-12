#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi2AP4STANetAnimExample");

int main(int argc, char *argv[])
{
    // LogLevel for debugging, comment/uncomment as needed
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    uint32_t nAps = 2;
    uint32_t nStas = 4;
    double simTime = 10.0;
    double distanceBetweenAps = 40.0;
    double apStaDistance = 15.0;

    // 1. Create node containers.
    NodeContainer wifiApNodes;
    wifiApNodes.Create(nAps);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nStas);

    // Map each STA to the nearest AP (2 STAs per AP)
    std::vector<uint32_t> staToAp = {0, 0, 1, 1};

    // 2. Configure Wi-Fi PHY and MAC
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

    WifiMacHelper mac;

    // Create a different SSID for each AP
    std::vector<Ssid> ssids;
    for (uint32_t i = 0; i < nAps; ++i) {
        std::ostringstream ssidStr;
        ssidStr << "network-" << i;
        ssids.push_back(Ssid(ssidStr.str()));
    }

    // Containers for device assignments
    NetDeviceContainer apDevices[nAps];
    NetDeviceContainer staDevices[nAps];

    // For each AP, install devices on the AP and its STAs
    for (uint32_t i = 0; i < nAps; ++i) {
        // AP MAC
        mac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssids[i]));
        apDevices[i] = wifi.Install(phy, mac, wifiApNodes.Get(i));

        // Collect the STAs for this AP
        NodeContainer stasForThisAp;
        for (uint32_t j = 0; j < nStas; ++j) {
            if (staToAp[j] == i) {
                stasForThisAp.Add(wifiStaNodes.Get(j));
            }
        }

        // STA MAC
        mac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssids[i]),
                    "ActiveProbing", BooleanValue(false));
        staDevices[i] = wifi.Install(phy, mac, stasForThisAp);
    }

    // 3. Mobility: Place APs and STAs in a line, each STA closer to its AP
    MobilityHelper mobility;

    // Position APs
    Ptr<ListPositionAllocator> apPositionAlloc = CreateObject<ListPositionAllocator>();
    apPositionAlloc->Add(Vector(0.0, 0.0, 0.0));
    apPositionAlloc->Add(Vector(distanceBetweenAps, 0.0, 0.0));
    mobility.SetPositionAllocator(apPositionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNodes);

    // Position STAs: two per AP, offset in y-direction
    Ptr<ListPositionAllocator> staPositionAlloc = CreateObject<ListPositionAllocator>();
    staPositionAlloc->Add(Vector(0.0 + apStaDistance,  5.0, 0.0)); // STA 0 near AP 0
    staPositionAlloc->Add(Vector(0.0 + apStaDistance, -5.0, 0.0)); // STA 1 near AP 0
    staPositionAlloc->Add(Vector(distanceBetweenAps - apStaDistance, 5.0, 0.0)); // STA 2 near AP 1
    staPositionAlloc->Add(Vector(distanceBetweenAps - apStaDistance, -5.0, 0.0)); // STA 3 near AP 1
    mobility.SetPositionAllocator(staPositionAlloc);
    mobility.Install(wifiStaNodes);

    // 4. Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNodes);
    stack.Install(wifiStaNodes);

    // 5. Assign IP addresses
    Ipv4AddressHelper address;

    std::vector<Ipv4InterfaceContainer> staInterfaces(nAps);
    std::vector<Ipv4InterfaceContainer> apInterfaces(nAps);

    for (uint32_t i = 0; i < nAps; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        staInterfaces[i] = address.Assign(staDevices[i]);
        apInterfaces[i] = address.Assign(apDevices[i]);
        address.NewNetwork();
    }

    // 6. Applications: UDP traffic between STAs on different APs
    uint16_t port = 9;
    ApplicationContainer serverApps, clientApps;

    // For demonstration, let each STA send UDP traffic to a STA on the other AP
    // STA 0 <-> STA 2, STA 1 <-> STA 3
    for (uint32_t i = 0; i < 2; ++i) {
        uint32_t srcStaIdx = i;
        uint32_t dstStaIdx = i + 2;
        // Setup echo server on destination
        UdpEchoServerHelper echoServer(port + i);
        // destination STA node
        ApplicationContainer server = echoServer.Install(wifiStaNodes.Get(dstStaIdx));
        server.Start(Seconds(1.0));
        server.Stop(Seconds(simTime));
        serverApps.Add(server);

        // Setup echo client on source
        UdpEchoClientHelper echoClient(staInterfaces[staToAp[dstStaIdx]].GetAddress(i, 0), port + i);
        echoClient.SetAttribute("MaxPackets", UintegerValue(10));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer client = echoClient.Install(wifiStaNodes.Get(srcStaIdx));
        client.Start(Seconds(2.0));
        client.Stop(Seconds(simTime));
        clientApps.Add(client);
    }

    // 7. NetAnim configuration
    AnimationInterface anim("wifi-2ap-4sta.xml");

    // Set node descriptions and colors for NetAnim
    for (uint32_t i = 0; i < nAps; ++i) {
        anim.UpdateNodeDescription(wifiApNodes.Get(i), "AP" + std::to_string(i));
        anim.UpdateNodeColor(wifiApNodes.Get(i), 255, 0, 0); // Red for APs
    }
    for (uint32_t i = 0; i < nStas; ++i) {
        anim.UpdateNodeDescription(wifiStaNodes.Get(i), "STA" + std::to_string(i));
        anim.UpdateNodeColor(wifiStaNodes.Get(i), 0, 0, 255); // Blue for STAs
    }

    // Enable packet metadata for NetAnim arrow animation
    Config::SetDefault("ns3::Ipv4L3Protocol::DefaultTtl", UintegerValue(64));

    // 8. Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}