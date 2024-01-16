#pragma once

// === Openness ===
// High:
//   - Very creative
//   - Open to new ideas
//   - Focused on new challenges
//   - Happy thinking abstractly
//   - Curious
// Low :
//   - Dislikes change
//   - Does not enjoy new things
//   - Resists new ideas
//   - Not imaginative
//   - Dislikes abstract or theoretical concepts

// === Conscientiousness ===
// High:
//   - Spends time preparing
//   - Finishes important tasks right away
//   - Pays attention to details
//   - Likes to keep things tidy
//   - Likes schedules
// Low:
//   - Dislikes structure and schedules
//   - Makes messes and doesn't take care of things
//   - Fails to return things put away
//   - Shirks duties
//   - Fails to complete necessary or assigned tasks

// === Extraversion ===
// High:
//   - Enjoys being the center of attention
//   - Likes to start conversations
//   - Likes to talk
//   - Enjoys parties and large gatherings
//   - Enjoys meeting new people
//   - Feels energized when around others
// Low:
//   - Prefers solitude
//   - Dislikes making small talk
//   - Dislikes being the center of attention
//   - Thinks before speaking or acting
//   - Feels exhausted from having to socialize a lot
//   - Finds it difficult to start conversations

// === Agreeableness ===
// High:
//   - Is interested in people
//   - Sympathizes with others' feelings
//   - Has a soft heart
//   - Takes time out for others
//   - Feels others' emotions
//   - Assists those in need of help
//   - Willing to make sacrifices for others
// Low:
//   - Takes little interest in others
//   - Feels little sympathy for others
//   - Is not really interested in others' problems
//   - Insults and belittles others
//   - Manipulates others to get own way

// === Neuroticism ===
// High:
//   - Experiences a lot of stress
//   - Worries about many different things
//   - Gets upset easily
//   - Dramatic mood shifts
//   - Gets anxious
//   - Struggles to bounce back after stressful events
// Low:
//   - Emotionally stable
//   - Deals well with stress
//   - Rarely feels sad or depressed
//   - Doesn't worry much
//   - Very relaxed

struct PersonalityComponent {
    ui8 openness = 0;
    ui8 concientiousness = 0;
    ui8 extraversion = 0;
    ui8 agreeableness = 0;
    ui8 neuroticism = 0;
};
